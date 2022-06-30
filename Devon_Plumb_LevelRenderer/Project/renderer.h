// minimalistic code to draw a single triangle, this is not part of the API.
#include "h2bParser.h"
#include <string>
// Simple Vertex Shader
const char* vertexShaderSource = R"(
#version 330 // GLSL 3.30
// an ultra simple glsl vertex shader
struct OBJ_ATTRIBUTES
{
	vec3 Kd; // diffuse reflectivity
	float d; // dissolve (transparency)
	vec3 Ks; // specular reflectivity
	float Ns; // specular exponent
	vec3 Ka; // ambient reflectivity
	float sharpness; // local reflection map sharpness
	vec3 Tf; // transmission filter
	float Ni; // optical density (index of refraction)
	vec3 Ke; // emissive reflectivity
	uint illum; // illumination model
};
layout(row_major) uniform UBO_DATA
{
	vec4 sunDirection, sunColor;
	mat4 viewMatrix, projectionMatrix;
	mat4 worldMatrix;
	OBJ_ATTRIBUTES material;
};
in vec3 local_pos;
void main()
{
	vec4 mathResult = vec4(local_pos,1) * worldMatrix * viewMatrix * projectionMatrix;
	gl_Position = mathResult;
}
)";
// Simple Fragment Shader
const char* fragmentShaderSource = R"(
#version 330 // GLSL 3.30
// an ultra simple glsl fragment shader
struct OBJ_ATTRIBUTES
{
	vec3 Kd; // diffuse reflectivity
	float d; // dissolve (transparency)
	vec3 Ks; // specular reflectivity
	float Ns; // specular exponent
	vec3 Ka; // ambient reflectivity
	float sharpness; // local reflection map sharpness
	vec3 Tf; // transmission filter
	float Ni; // optical density (index of refraction)
	vec3 Ke; // emissive reflectivity
	uint illum; // illumination model
};
layout(row_major) uniform UBO_DATA
{
	vec4 sunDirection, sunColor;
	mat4 viewMatrix, projectionMatrix;
	mat4 worldMatrix;
	OBJ_ATTRIBUTES material;
};
void main() 
{	
	gl_FragColor = vec4(material.Kd, 1); // TODO: Part 1a
}
)";
// Used to print debug infomation from OpenGL, pulled straight from the official OpenGL wiki.
#ifndef NDEBUG
void MessageCallback(GLenum source, GLenum type, GLuint id,
	GLenum severity, GLsizei length,
	const GLchar* message, const void* userParam) {
	fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
		(type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);
}
#endif
// Creation, Rendering & Cleanup
class Renderer
{
	// proxy handles
	GW::SYSTEM::GWindow win;
	GW::GRAPHICS::GOpenGLSurface ogl;
	// what we need at a minimum to draw a triangle
	GLuint vertexArray = 0;
	GLuint vertexBufferObject = 0;

	// Create index buffer, vertex shader, and shader
	GLuint indexBuffer = 0;
	GLuint vertexShader = 0;
	GLuint fragmentShader = 0;
	GLuint shaderExecutable = 0;

	// Create render buffer and Gateware input proxies
	GLuint renderBuffer = 0;
	GW::INPUT::GInput input;
	GW::INPUT::GController controller;

	// Create Gateware variables
	GW::MATH::GMatrix proxy;
	GW::MATH::GMATRIXF worldMatrix = GW::MATH::GIdentityMatrixF;
	GW::MATH::GMATRIXF viewMatrix = GW::MATH::GIdentityMatrixF;
	GW::MATH::GMATRIXF projectionMatrix = GW::MATH::GIdentityMatrixF;
	GW::MATH::GVECTORF lightDir = { -1.0f, -1.0f, -2.0f };
	GW::MATH::GVECTORF lightColor = { 0.9f, 0.9f, 1.0f, 1.0f };

	struct parsedModels
	{
		std::string fileName;
		H2B::Parser parser;
	};
	std::vector<parsedModels> parsers;

	std::vector<H2B::VERTEX> vertices;
	std::vector<unsigned int> indices02;

	std::vector<GW::MATH::GMATRIXF> matrices; // Vector to hold our matrices

	struct _UBO_DATA
	{
		GW::MATH::GVECTORF sunDirection, sunColor;
		GW::MATH::GMATRIXF viewMatrix, projectionMatrix;
		GW::MATH::GMATRIXF worldMatrix;
		H2B::ATTRIBUTES material;
	};
	_UBO_DATA renderData;

public:
	Renderer(GW::SYSTEM::GWindow _win, GW::GRAPHICS::GOpenGLSurface _ogl)
	{
		win = _win;
		ogl = _ogl;

		// Setup proxies and matrices
		proxy.Create();
		input.Create(win);
		controller.Create();
		proxy.LookAtLHF(GW::MATH::GVECTORF{ 0.75f, 0.25f, -1.5f }, GW::MATH::GVECTORF{ 0.15f, 0.75f, 0 }, GW::MATH::GVECTORF{ 0, 1.0f, 0 }, viewMatrix);
		proxy.ProjectionOpenGLLHF(G_DEGREE_TO_RADIAN(65), 800.f / 600.0f, 0.1f, 100.0f, projectionMatrix);
		GW::MATH::GVector::NormalizeF(lightDir, lightDir);

		// Push data into renderData struct
		renderData.worldMatrix = worldMatrix;
		renderData.viewMatrix = viewMatrix;
		renderData.projectionMatrix = projectionMatrix;
		renderData.sunDirection = lightDir;
		renderData.sunColor = lightColor;

		// Link Needed OpenGL API functions
		LoadExtensions();
		// In debug mode we link openGL errors to the console
#ifndef NDEBUG
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(MessageCallback, 0);
#endif
		std::ifstream file;
		std::ifstream modelFiles;
		int meshCompareResult = 5; // Set to 5 just for conditional
		int matrixCompareResult = 5; // Set to 5 just for conditional
		std::string h2bFile;
		H2B::Parser parser;
		GW::MATH::GMATRIXF matrix = GW::MATH::GIdentityMatrixF; // Make a matrix to fill our values into

		file.open("../GameLevel.txt", std::ifstream::in);

		std::string filePath = "../Models/";

		// Parse file
		if (file.is_open())
		{
			std::string line;
			while (getline(file, line))
			{
				std::cout << line << std::endl;
				if (matrixCompareResult <= 3)
				{
					// Grab the start and end of our float values
					std::size_t startingPos = line.find("(");

					std::string nums = line.substr(startingPos + 1);

					nums.erase(remove(nums.begin(), nums.end(), ' '), nums.end());

					// Grab first float
					std::string::size_type st;
					float a = std::stof(nums, &st); // This line correctly grabs the first value of the matrix (row 1)

					// Ugly way to push values into matrix row1
					if (matrixCompareResult == 0)
					{
						matrix.row1.x = a;
					}
					else if (matrixCompareResult == 1)
					{
						matrix.row2.x = a;
					}
					else if (matrixCompareResult == 2)
					{
						matrix.row3.x = a;
					}
					else if (matrixCompareResult == 3)
					{
						matrix.row4.x = a;
					}

					// Grab second float
					std::size_t commaPos = line.find(",");
					nums = line.substr(commaPos + 1);
					float b = std::stof(nums);

					// Ugly way to push values into matrix row1
					if (matrixCompareResult == 0)
					{
						matrix.row1.y = b;
					}
					else if (matrixCompareResult == 1)
					{
						matrix.row2.y = b;
					}
					else if (matrixCompareResult == 2)
					{
						matrix.row3.y = b;
					}
					else if (matrixCompareResult == 3)
					{
						matrix.row4.y = b;
					}

					// Grab third float
					std::size_t commaPos02 = line.find(",", commaPos + 1);
					nums = line.substr(commaPos02 + 1);
					float c = std::stof(nums);

					// Ugly way to push values into matrix row1
					if (matrixCompareResult == 0)
					{
						matrix.row1.z = c;
					}
					else if (matrixCompareResult == 1)
					{
						matrix.row2.z = c;
					}
					else if (matrixCompareResult == 2)
					{
						matrix.row3.z = c;
					}
					else if (matrixCompareResult == 3)
					{
						matrix.row4.z = c;
					}

					// Grab fourth float
					std::size_t commaPos03 = line.find(",", commaPos02 + 1);
					nums = line.substr(commaPos03 + 1);
					float d = std::stof(nums);

					// Ugly way to push values into matrix row1
					if (matrixCompareResult == 0)
					{
						matrix.row1.w = d;
					}
					else if (matrixCompareResult == 1)
					{
						matrix.row2.w = d;
					}
					else if (matrixCompareResult == 2)
					{
						matrix.row3.w = d;
					}
					else if (matrixCompareResult == 3)
					{
						matrix.row4.w = d;
						matrices.push_back(matrix); // Push back the matrix here since we just grabbed the last value needed
					}

					matrixCompareResult++; // Increment matrixCompareResult so we don't enter this if statement after 4 loops
				}
				if (meshCompareResult == 0)
				{
					parsedModels pM;
					h2bFile = line; // Grab the name of the mesh for our h2b file
					h2bFile += ".h2b";
					filePath += h2bFile;
					pM.fileName = h2bFile;
					parser.Parse(filePath.c_str());
					pM.parser = parser;
					parsers.push_back(pM);
					filePath = "../Models/";

					meshCompareResult = 5; // Set compareResult back to 5 so we don't enter this if statement next iteration
					matrixCompareResult = 0; // Set matrixCompareResult to 0 so we enter the next if statement
				}
				if (line.compare("MESH") == 0)
				{
					std::cout << "MATRIX DATA: " << std::endl;
					std::cout << matrix.row1.x << ", " << matrix.row1.y << ", " << matrix.row1.z << ", " << matrix.row1.w << std::endl;
					std::cout << matrix.row2.x << ", " << matrix.row2.y << ", " << matrix.row2.z << ", " << matrix.row2.w << std::endl;
					std::cout << matrix.row3.x << ", " << matrix.row3.y << ", " << matrix.row3.z << ", " << matrix.row3.w << std::endl;
					std::cout << matrix.row4.x << ", " << matrix.row4.y << ", " << matrix.row4.z << ", " << matrix.row4.w << std::endl;
					meshCompareResult = 0;
				}
			}
			file.close();
		}

		// This should give all vertex and index data for every model
		for (size_t j = 0; j < parsers.size(); j++)
		{
			int verticesSize = vertices.size();
			for (size_t i = 0; i < parsers[j].parser.vertexCount; i++)
			{
				vertices.push_back(parsers[j].parser.vertices[i]);
			}

			parsers[j].parser.indexOffset = indices02.size();

			for (size_t i = 0; i < parsers[j].parser.indexCount; i++)
			{
				indices02.push_back(parsers[j].parser.indices[i] + verticesSize);
			}
		}

		for (size_t i = 0; i < parsers.size(); i++)
		{
			std::cout << "File Name: " << parsers[i].fileName << " Index Count: " << parsers[i].parser.indexCount << std::endl;
		}

		glGenVertexArrays(1, &vertexArray);
		glGenBuffers(1, &vertexBufferObject);
		glBindVertexArray(vertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(H2B::VERTEX), &vertices.front(), GL_STATIC_DRAW);
		// Setup index buffer
		glGenBuffers(1, &indexBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices02.size() * sizeof(GLuint), &indices02.front(), GL_STATIC_DRAW);
		// Setup render buffer
		glGenBuffers(1, &renderBuffer);
		glBindBuffer(GL_UNIFORM_BUFFER, renderBuffer);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(renderData), &renderData, GL_DYNAMIC_DRAW);
		// Create Vertex Shader
		vertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
		glCompileShader(vertexShader);
		char errors[1024]; GLint result;
		glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &result);
		if (result == false)
		{
			glGetShaderInfoLog(vertexShader, 1024, NULL, errors);
			std::cout << errors << std::endl;
		}
		// Create Fragment Shader
		fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
		glCompileShader(fragmentShader);
		glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &result);
		if (result == false)
		{
			glGetShaderInfoLog(fragmentShader, 1024, NULL, errors);
			std::cout << errors << std::endl;
		}
		// Create Executable Shader Program
		shaderExecutable = glCreateProgram();
		glAttachShader(shaderExecutable, vertexShader);
		glAttachShader(shaderExecutable, fragmentShader);
		glLinkProgram(shaderExecutable);
		glGetProgramiv(shaderExecutable, GL_LINK_STATUS, &result);
		if (result == false)
		{
			glGetProgramInfoLog(shaderExecutable, 1024, NULL, errors);
			std::cout << errors << std::endl;
		}
	}
	void UpdateCamera()
	{
		// Chrono Info
		std::chrono::steady_clock::time_point callTimeFromLastCall;
		std::chrono::steady_clock::time_point callTimeFromCurrentCall;

		// Matrices and Vector
		GW::MATH::GVECTORF horizontalVector;
		GW::MATH::GMATRIXF tempViewMatrix;
		GW::MATH::GMATRIXF tempWorldMatrix;

		// FOV, deltaTime, and Changes
		double dT = 0; // dT = deltaTime
		float fov = G_DEGREE_TO_RADIAN(65);
		float zChange;
		float xChange;

		// Window
		GLfloat aspectRatio;
		unsigned int windowWidth;
		unsigned int windowHeight;

		// Get the aspect ratio of the window
		ogl.GetAspectRatio(aspectRatio);

		// Get the width of the window
		win.GetWidth(windowWidth);

		// Get the height of the window
		win.GetHeight(windowHeight);

		// Mouse
		float sensitivityOfMouse = 40.0f;
		float mouseDX; // Mouse deltaX
		float mouseDY; // Mouse deltaY

		// Orientation
		float leftRight;
		float upDown;

		// Speed
		float speedPerFrame;
		float speedOfCamera = 10.0f;

		// Left and right keys
		float dKey = 0;
		float aKey = 0;

		// Forward and back keys
		float wKey = 0;
		float sKey = 0;

		// Mouse up
		float mouseUp;

		// Mouse down
		float mouseLeft;

		// Get time from chrono and from frame to frame
		callTimeFromCurrentCall = std::chrono::steady_clock::now();
		dT = std::chrono::duration_cast<std::chrono::milliseconds>(callTimeFromCurrentCall - callTimeFromLastCall).count() / 100000000000.0f;
		callTimeFromLastCall = callTimeFromCurrentCall;

		// Apply our camera speed with change
		speedPerFrame = speedOfCamera * dT;

		// Inverse the view matrix
		proxy.InverseF(renderData.viewMatrix, tempViewMatrix);

		// Handle right and forward
		input.GetState(G_KEY_D, wKey);
		input.GetState(G_KEY_W, aKey);

		// Handle left and back
		input.GetState(G_KEY_A, sKey);
		input.GetState(G_KEY_S, dKey);

		// Determine change on the X and Z axis
		zChange = aKey - dKey;
		xChange = wKey - sKey;

		// Setup our vector
		horizontalVector =
		{
			xChange * speedPerFrame,
			0,
			zChange * speedPerFrame
		};

		// Translate matrix locally rather than globally
		proxy.TranslateLocalF(tempViewMatrix, horizontalVector, tempViewMatrix);

		// Get mouse delta
		GW::GReturn result = input.GetMouseDelta(mouseDX, mouseDY);

		// If we detect movement from the mouse, run code to camera
		if (G_PASS(result) && result != GW::GReturn::REDUNDANT)
		{
			// Use the data we grabbed to apply mouse up/down movement
			upDown = fov * aspectRatio * mouseDX / windowWidth;
			tempWorldMatrix = tempViewMatrix;
			mouseUp = G_DEGREE_TO_RADIAN(upDown * sensitivityOfMouse);

			// Rotate and apply to matrix
			proxy.RotateYGlobalF(tempViewMatrix, mouseUp, tempViewMatrix);
			tempViewMatrix.row4 = tempWorldMatrix.row4;

			// Use the data we grabbed to apply mouse left/right movement
			leftRight = fov * mouseDY / windowHeight;
			mouseLeft = G_DEGREE_TO_RADIAN(leftRight * sensitivityOfMouse);

			// Rotate our view matrix locally rather than globally
			proxy.RotateXLocalF(tempViewMatrix, mouseLeft, tempViewMatrix);
		}

		// Inverse our view matrix
		proxy.InverseF(tempViewMatrix, renderData.viewMatrix);
	}
	void Render()
	{
		for (size_t i = 0; i < parsers.size(); i++)
		{
			renderData.worldMatrix = matrices[i];
			// setup the pipeline
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
			glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(H2B::VERTEX), &vertices.front(), GL_STATIC_DRAW);
			glUseProgram(shaderExecutable);
			// now we can draw
			glBindVertexArray(vertexArray);
			glUseProgram(shaderExecutable);
			// Bind our buffers and buffer data
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices02.size() * sizeof(GLuint), &indices02.front(), GL_STATIC_DRAW);
			// Get uniform block index
			GLuint index;
			index = glGetUniformBlockIndex(shaderExecutable, "UBO_DATA");
			// Bind buffer base
			glBindBufferBase(GL_UNIFORM_BUFFER, 0, renderBuffer);
			// Call uniform block binding
			glUniformBlockBinding(shaderExecutable, index, 0);

			for (unsigned k = 0; k < parsers[i].parser.meshCount; ++k)
			{
				renderData.material = parsers[i].parser.materials[parsers[i].parser.meshes[k].materialIndex].attrib;
				memcpy(glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY), &renderData, sizeof(_UBO_DATA));
				glUnmapBuffer(GL_UNIFORM_BUFFER);
				unsigned indexOffset = parsers[i].parser.batches[k].indexOffset + parsers[i].parser.indexOffset;
				glDrawElements(GL_TRIANGLES, parsers[i].parser.batches[k].indexCount, GL_UNSIGNED_INT, (void*)(indexOffset * sizeof(GLuint)));
			}

			// some video cards need this set back to zero or they won't display
			glBindVertexArray(0);
		}
	}
	~Renderer()
	{
		// free resources
		glDeleteVertexArrays(1, &vertexArray);
		glDeleteBuffers(1, &vertexBufferObject);
		glDeleteBuffers(1, &indexBuffer);
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		glDeleteProgram(shaderExecutable);
		glDeleteBuffers(1, &renderBuffer);
	}
private:
	// Modern OpenGL API Functions must be queried before use
	PFNGLCREATESHADERPROC				glCreateShader = nullptr;
	PFNGLSHADERSOURCEPROC				glShaderSource = nullptr;
	PFNGLCOMPILESHADERPROC				glCompileShader = nullptr;
	PFNGLGETSHADERIVPROC				glGetShaderiv = nullptr;
	PFNGLGETSHADERINFOLOGPROC			glGetShaderInfoLog = nullptr;
	PFNGLATTACHSHADERPROC				glAttachShader = nullptr;
	PFNGLDETACHSHADERPROC				glDetachShader = nullptr;
	PFNGLDELETESHADERPROC				glDeleteShader = nullptr;
	PFNGLCREATEPROGRAMPROC				glCreateProgram = nullptr;
	PFNGLLINKPROGRAMPROC				glLinkProgram = nullptr;
	PFNGLUSEPROGRAMPROC					glUseProgram = nullptr;
	PFNGLGETPROGRAMIVPROC				glGetProgramiv = nullptr;
	PFNGLGETPROGRAMINFOLOGPROC			glGetProgramInfoLog = nullptr;
	PFNGLGENVERTEXARRAYSPROC			glGenVertexArrays = nullptr;
	PFNGLBINDVERTEXARRAYPROC			glBindVertexArray = nullptr;
	PFNGLGENBUFFERSPROC					glGenBuffers = nullptr;
	PFNGLBINDBUFFERPROC					glBindBuffer = nullptr;
	PFNGLBUFFERDATAPROC					glBufferData = nullptr;
	PFNGLENABLEVERTEXATTRIBARRAYPROC	glEnableVertexAttribArray = nullptr;
	PFNGLDISABLEVERTEXATTRIBARRAYPROC	glDisableVertexAttribArray = nullptr;
	PFNGLVERTEXATTRIBPOINTERPROC		glVertexAttribPointer = nullptr;
	PFNGLGETUNIFORMLOCATIONPROC			glGetUniformLocation = nullptr;
	PFNGLUNIFORMMATRIX4FVPROC			glUniformMatrix4fv = nullptr;
	PFNGLDELETEBUFFERSPROC				glDeleteBuffers = nullptr;
	PFNGLDELETEPROGRAMPROC				glDeleteProgram = nullptr;
	PFNGLDELETEVERTEXARRAYSPROC			glDeleteVertexArrays = nullptr;
	PFNGLDEBUGMESSAGECALLBACKPROC		glDebugMessageCallback = nullptr;
	PFNGLGETUNIFORMBLOCKINDEXPROC		glGetUniformBlockIndex = nullptr;
	PFNGLBINDBUFFERBASEPROC				glBindBufferBase = nullptr;
	PFNGLUNIFORMBLOCKBINDINGPROC		glUniformBlockBinding = nullptr;
	PFNGLMAPBUFFERPROC					glMapBuffer = nullptr;
	PFNGLUNMAPBUFFERPROC				glUnmapBuffer = nullptr;

	// Modern OpenGL API functions need to be queried
	void LoadExtensions()
	{
		ogl.QueryExtensionFunction(nullptr, "glCreateShader", (void**)&glCreateShader);
		ogl.QueryExtensionFunction(nullptr, "glShaderSource", (void**)&glShaderSource);
		ogl.QueryExtensionFunction(nullptr, "glCompileShader", (void**)&glCompileShader);
		ogl.QueryExtensionFunction(nullptr, "glGetShaderiv", (void**)&glGetShaderiv);
		ogl.QueryExtensionFunction(nullptr, "glGetShaderInfoLog", (void**)&glGetShaderInfoLog);
		ogl.QueryExtensionFunction(nullptr, "glAttachShader", (void**)&glAttachShader);
		ogl.QueryExtensionFunction(nullptr, "glDetachShader", (void**)&glDetachShader);
		ogl.QueryExtensionFunction(nullptr, "glDeleteShader", (void**)&glDeleteShader);
		ogl.QueryExtensionFunction(nullptr, "glCreateProgram", (void**)&glCreateProgram);
		ogl.QueryExtensionFunction(nullptr, "glLinkProgram", (void**)&glLinkProgram);
		ogl.QueryExtensionFunction(nullptr, "glUseProgram", (void**)&glUseProgram);
		ogl.QueryExtensionFunction(nullptr, "glGetProgramiv", (void**)&glGetProgramiv);
		ogl.QueryExtensionFunction(nullptr, "glGetProgramInfoLog", (void**)&glGetProgramInfoLog);
		ogl.QueryExtensionFunction(nullptr, "glGenVertexArrays", (void**)&glGenVertexArrays);
		ogl.QueryExtensionFunction(nullptr, "glBindVertexArray", (void**)&glBindVertexArray);
		ogl.QueryExtensionFunction(nullptr, "glGenBuffers", (void**)&glGenBuffers);
		ogl.QueryExtensionFunction(nullptr, "glBindBuffer", (void**)&glBindBuffer);
		ogl.QueryExtensionFunction(nullptr, "glBufferData", (void**)&glBufferData);
		ogl.QueryExtensionFunction(nullptr, "glEnableVertexAttribArray", (void**)&glEnableVertexAttribArray);
		ogl.QueryExtensionFunction(nullptr, "glDisableVertexAttribArray", (void**)&glDisableVertexAttribArray);
		ogl.QueryExtensionFunction(nullptr, "glVertexAttribPointer", (void**)&glVertexAttribPointer);
		ogl.QueryExtensionFunction(nullptr, "glGetUniformLocation", (void**)&glGetUniformLocation);
		ogl.QueryExtensionFunction(nullptr, "glUniformMatrix4fv", (void**)&glUniformMatrix4fv);
		ogl.QueryExtensionFunction(nullptr, "glDeleteBuffers", (void**)&glDeleteBuffers);
		ogl.QueryExtensionFunction(nullptr, "glDeleteProgram", (void**)&glDeleteProgram);
		ogl.QueryExtensionFunction(nullptr, "glDeleteVertexArrays", (void**)&glDeleteVertexArrays);
		ogl.QueryExtensionFunction(nullptr, "glDebugMessageCallback", (void**)&glDebugMessageCallback);
		ogl.QueryExtensionFunction(nullptr, "glGetUniformBlockIndex", (void**)&glGetUniformBlockIndex);
		ogl.QueryExtensionFunction(nullptr, "glBindBufferBase", (void**)&glBindBufferBase);
		ogl.QueryExtensionFunction(nullptr, "glUniformBlockBinding", (void**)&glUniformBlockBinding);
		ogl.QueryExtensionFunction(nullptr, "glMapBuffer", (void**)&glMapBuffer);
		ogl.QueryExtensionFunction(nullptr, "glUnmapBuffer", (void**)&glUnmapBuffer);
	}
};
