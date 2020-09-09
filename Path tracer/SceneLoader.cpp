#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <cfloat>
#include <string.h>
#include <assert.h>

#include "linear_math.h"
#include "Geometry.h"
#include "SceneLoader.h"


using std::string;

unsigned verticesNo = 0;
unsigned trianglesNo = 0;
Vertex* vertices = NULL;   // vertex list
Triangle* triangles = NULL;  // triangle list


struct face {
	std::vector<int> vertex;
	std::vector<int> texture;
	std::vector<int> normal;
};

std::vector<face> faces;

namespace enums {
	enum ColorComponent {
		Red = 0,
		Green = 1,
		Blue = 2
	};
}

using namespace enums;

// Rescale input objects to have this size...
const float MaxCoordAfterRescale = 0.4f;

// if some file cannot be found, panic and exit
void panic(const char* fmt, ...)
{
	static char message[131072];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(message, sizeof message, fmt, ap);
	printf(message); fflush(stdout);
	va_end(ap);

	exit(1);
}

struct TriangleMesh
{
	std::vector<Vec3f> verts;
	std::vector<Vec3i> faces;
	Vec3f bounding_box[2];   // mesh bounding box
};

void load_object(const char* filename)
{
	std::cout << "Loading object..." << std::endl;
	const char* edot = strrchr(filename, '.');
	if (edot) {
		edot++;

		// Stanford PLY models
		if (!strcmp(edot, "PLY") || !strcmp(edot, "ply")) {
			// Only shadevis generated objects, not full blown parser!
			std::ifstream file(filename, std::ios::in);
			if (!file) {
				panic((string("Missing ") + string(filename)).c_str());
			}

			Vertex* pCurrentVertex = NULL;
			Triangle* pCurrentTriangle = NULL;

			string line;
			unsigned totalVertices, totalTriangles, lineNo = 0;
			bool inside = false;
			while (getline(file, line)) {
				lineNo++;
				if (!inside) {
					if (line.substr(0, 14) == "element vertex") {
						std::istringstream str(line);
						string word1;
						str >> word1;
						str >> word1;
						str >> totalVertices;
						vertices = (Vertex*)malloc(totalVertices * sizeof(Vertex));
						verticesNo = totalVertices;
						pCurrentVertex = vertices;
					}
					else if (line.substr(0, 12) == "element face") {
						std::istringstream str(line);
						string word1;
						str >> word1;
						str >> word1;
						str >> totalTriangles;
						triangles = (Triangle*)malloc(totalTriangles * sizeof(Triangle));
						trianglesNo = totalTriangles;
						pCurrentTriangle = triangles;
					}
					else if (line.substr(0, 10) == "end_header")
						inside = true;
				}
				else {
					if (totalVertices) {

						totalVertices--;
						float x, y, z;

						std::istringstream str_in(line);
						str_in >> x >> y >> z;

						pCurrentVertex->x = x;
						pCurrentVertex->y = y;
						pCurrentVertex->z = z;
						pCurrentVertex->_normal.x = 0.f; // not used for now, normals are computed on-the-fly by GPU 
						pCurrentVertex->_normal.y = 0.f; // not used
						pCurrentVertex->_normal.z = 0.f; // not used 

						pCurrentVertex++;
					}

					else if (totalTriangles) {

						totalTriangles--;
						unsigned dummy;
						float r, g, b;
						unsigned idx1, idx2, idx3; // vertex index
						std::istringstream str2(line);
						if (str2 >> dummy >> idx1 >> idx2 >> idx3)
						{
							// set rgb colour to white
							r = 255; g = 255; b = 255;

							pCurrentTriangle->_idx1 = idx1;
							pCurrentTriangle->_idx2 = idx2;
							pCurrentTriangle->_idx3 = idx3;

							Vertex* vertexA = &vertices[idx1];
							Vertex* vertexB = &vertices[idx2];
							Vertex* vertexC = &vertices[idx3];
							pCurrentTriangle++;
						}
					}
				}
			}
		}  // end of ply loader code

		////////////////////
		// basic OBJ loader
		////////////////////
		else if (!strcmp(edot, "obj")) {

			std::cout << "loading OBJ model: " << filename;
			std::string filenamestring = filename;
			std::ifstream in(filenamestring.c_str());
			//std::cout << filenamestring << "\n";

			if (!in.good())
			{
				std::cout << "ERROR: loading obj:(" << filename << ") file not found or not good" << "\n";
				system("PAUSE");
				exit(0);
			}

			Vertex* pCurrentVertex = NULL;
			Triangle* pCurrentTriangle = NULL;
			unsigned totalVertices, totalTriangles = 0;
			TriangleMesh mesh;

			std::ifstream ifs(filenamestring.c_str(), std::ifstream::in);

			if (!ifs.good())
			{
				std::cout << "ERROR: loading obj:(" << filename << ") file not found or not good" << "\n";
				system("PAUSE");
				exit(0);
			}

			std::string line, key;
			while (!ifs.eof() && std::getline(ifs, line)) {
				key = "";
				std::stringstream stringstream(line);
				stringstream >> key >> std::ws;

				if (key == "v") { // vertex	
					float x, y, z;
					while (!stringstream.eof()) {
						stringstream >> x >> std::ws >> y >> std::ws >> z >> std::ws;
						mesh.verts.push_back(Vec3f(x, y, z));
					}
				}
				else if (key == "vp") {
					// Parameter space vertices
					float x;
					while (!stringstream.eof()) {
						stringstream >> x >> std::ws;
					}
				}
				else if (key == "vt") { 
					// texture coordinate
					float x;
					while (!stringstream.eof()) {
						stringstream >> x >> std::ws;
					}
				}
				else if (key == "vn") { 
					// Vertex normals
					float x;
					while (!stringstream.eof()) {
						stringstream >> x >> std::ws;
					}
				}

				else if (key == "f") { // face
					face f;
					int v, t, n;
					while (!stringstream.eof()) {
						stringstream >> v >> std::ws;
						f.vertex.push_back(v); // v - 1
						if (stringstream.peek() == '/') {
							stringstream.get();
							if (stringstream.peek() == '/') {
								stringstream.get();
								stringstream >> n >> std::ws;
								f.normal.push_back(n - 1);
							}
							else {
								stringstream >> t >> std::ws;
								f.texture.push_back(t - 1);
								if (stringstream.peek() == '/') {
									stringstream.get();
									stringstream >> n >> std::ws;
									f.normal.push_back(n - 1);
								}
							}
						}
					}

					int numtriangles = f.vertex.size() - 2; // 1 triangle if 3 vertices, 2 if 4 etc

					for (int i = 0; i < numtriangles; i++) {  // first vertex remains the same for all triangles in a triangle fan
						mesh.faces.push_back(Vec3i(f.vertex[0], f.vertex[i + 1], f.vertex[i + 2]));
					}
				}
				else {

				}

			} // end of while loop

			totalVertices = mesh.verts.size();
			totalTriangles = mesh.faces.size();

			vertices = new Vertex[totalVertices];
			verticesNo = totalVertices;
			pCurrentVertex = vertices;

			triangles = new Triangle[totalTriangles];
			trianglesNo = totalTriangles;
			pCurrentTriangle = triangles;

			std::cout << "total vertices: " << totalVertices << "\n";
			std::cout << "total triangles: " << totalTriangles << "\n";

			for (int i = 0; i < totalVertices; i++) {
				Vec3f currentvert = mesh.verts[i];
				pCurrentVertex->x = currentvert.x;
				pCurrentVertex->y = currentvert.y;
				pCurrentVertex->z = currentvert.z;
				pCurrentVertex->_normal.x = 0.f; // not used for now, normals are computed on-the-fly by GPU
				pCurrentVertex->_normal.y = 0.f; // not used 
				pCurrentVertex->_normal.z = 0.f; // not used 

				pCurrentVertex++;
			}

			std::cout << "Vertices loaded\n";

			while (totalTriangles)
			{
				totalTriangles--;

				Vec3i currentfaceinds = mesh.faces[totalTriangles];

				pCurrentTriangle->_idx1 = currentfaceinds.x - 1;
				pCurrentTriangle->_idx2 = currentfaceinds.y - 1;
				pCurrentTriangle->_idx3 = currentfaceinds.z - 1;

				Vertex* vertexA = &vertices[currentfaceinds.x - 1];
				Vertex* vertexB = &vertices[currentfaceinds.y - 1];
				Vertex* vertexC = &vertices[currentfaceinds.z - 1];

				pCurrentTriangle++;
			}
		}

		else
			panic("Unknown extension (only .ply and .obj accepted)");
	}
	else
		panic("No extension in filename (only .ply accepted)");

	std::cout << "Vertices:  " << verticesNo << std::endl;
	std::cout << "Triangles: " << trianglesNo << std::endl;

}

/////////////////////////
// SCENE GEOMETRY PROCESSING
/////////////////////////
float processgeo() {

	// Center scene at world's center

	Vec3f minp(FLT_MAX, FLT_MAX, FLT_MAX);
	Vec3f maxp(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	// calculate min and max bounds of scene
	// loop over all triangles in scene, grow minp and maxp
	for (unsigned i = 0; i < trianglesNo; i++) {

		minp = min3f(minp, vertices[triangles[i]._idx1]);
		minp = min3f(minp, vertices[triangles[i]._idx2]);
		minp = min3f(minp, vertices[triangles[i]._idx3]);

		maxp = max3f(maxp, vertices[triangles[i]._idx1]);
		maxp = max3f(maxp, vertices[triangles[i]._idx2]);
		maxp = max3f(maxp, vertices[triangles[i]._idx3]);
	}

	// scene bounding box center before scaling and translating
	Vec3f origCenter = Vec3f(
		(maxp.x + minp.x) * 0.5,
		(maxp.y + minp.y) * 0.5,
		(maxp.z + minp.z) * 0.5);

	minp -= origCenter;
	maxp -= origCenter;

	// Scale scene so max(abs x,y,z coordinates) = MaxCoordAfterRescale

	float maxi = 0;
	maxi = std::max(maxi, (float)fabs(minp.x));
	maxi = std::max(maxi, (float)fabs(minp.y));
	maxi = std::max(maxi, (float)fabs(minp.z));
	maxi = std::max(maxi, (float)fabs(maxp.x));
	maxi = std::max(maxi, (float)fabs(maxp.y));
	maxi = std::max(maxi, (float)fabs(maxp.z));

	std::cout << "Scaling factor: " << (MaxCoordAfterRescale / maxi) << "\n";
	std::cout << "Center origin: " << origCenter.x << " " << origCenter.y << " " << origCenter.z << "\n";

	std::cout << "\nCentering and scaling vertices..." << std::endl;
	for (unsigned i = 0; i < verticesNo; i++) {
		vertices[i] -= origCenter;
		
		// change the size
		//vertices[i] *= (MaxCoordAfterRescale / maxi);
		vertices[i] *= 0.01; // 0.25
	}

	return MaxCoordAfterRescale;
}