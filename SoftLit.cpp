#include "stdafx.h"

#include "Rasterizer.h"
#include "Primitive.h"
#include "Display.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define WIDTH 1280
#define HEIGHT 720

using namespace std;
using namespace glm;
using namespace softlit;

//#define SINGLE_FRAME_OUTPUT

struct mat_ubo
{
	mat3 NORMAL;
	mat4 MVP;
};

// VS
vec4 VS_Simple(const glm::vec3& pos, mat_ubo* ubo, const Vertex_IN* const in, Vertex_OUT* out)
{
	vec3 normal = normalize(in->attrib_vec3[0]);
	out->PushVertexAttribute(normal);

	return (ubo->MVP * vec4(pos, 1));
}

// FS
vec4 FS_Simple(mat_ubo* ubo, const Vertex_OUT* const in)
{
	// Simply color normals
	return vec4(abs(in->attrib_vec3[0]), 1);
}

void ImportScene(vector<Primitive*>& objects, const string&);

int main(int argc, char* argv[])
{
	float fov = 60.f;
	uint32_t width = WIDTH;
	uint32_t height = HEIGHT;

	vector<Primitive*> objects;

	ImportScene(objects, "../cube.obj");

	DBG_ASSERT(!objects.empty() && "Failed to import models!");

	RasterizerSetup rasterSetup;
	rasterSetup.cullMode = CullMode::CULL_DISABLED;
	rasterSetup.vertexWinding = VertexWinding::CLOCKWISE;
	rasterSetup.viewport = { 0u, 0u, width, height };

	Rasterizer* rasterizer = new Rasterizer(rasterSetup);

	vec3 eye(0, 0, -5);
	vec3 lookat(0, 0, 0);
	vec3 up(0, 1, 0);

	mat4 view = lookAtRH(eye, lookat, up);
	mat4 proj = perspectiveRH(glm::radians(fov), (float)width / (float)height, 0.5f, 100.f);
	mat4 model;

	// Create primitive shading data
	const auto VS = reinterpret_cast<vertex_shader> (&VS_Simple);
	const auto FS = reinterpret_cast<fragment_shader> (&FS_Simple);

	mat_ubo ubo;

	for (Primitive* prim : objects)
	{
		prim->VS(VS);
		prim->FS(FS);
		prim->UBO(&ubo);
	}

#ifdef SINGLE_FRAME_OUTPUT
	for (Primitive* prim : objects)
	{
		//model = rotate(model, 0.025f, vec3(0, 1, 0));
		mat4 mv = view * model;
		mat3 normal = { mv[0], mv[1], mv[2] };
		mat4 mvp = proj * mv;

		mat_ubo* ubo = static_cast<mat_ubo*> (prim->UBO());
		ubo->MVP = mvp;
		ubo->NORMAL = normal;

		rasterizer->Draw(prim);
	}

	const vector<vec4>& frameBuffer = rasterizer->getFrameBuffer();
	FILE *f = NULL;
	fopen_s(&f, "../render.ppm", "w");
	fprintf(f, "P3\n%d %d\n%d\n ", WIDTH, HEIGHT, 255);
	for (int32_t i = 0; i < WIDTH * HEIGHT; ++i)
	{
		uint r = (255 * frameBuffer[i].x);
		uint g = (255 * frameBuffer[i].y);
		uint b = (255 * frameBuffer[i].z);
		fprintf(f, "%d %d %d ", r, g, b);
	}
	fclose(f);

#else
	// Init SDL
	Display display(width, height, false);

	SDL_Event event;
	bool running = true;
	while (running)
	{
		while (SDL_PollEvent(&event))
		{
			if ((SDL_QUIT == event.type) ||
				(SDL_KEYDOWN == event.type && SDL_SCANCODE_ESCAPE == event.key.keysym.scancode))
			{
				running = false;
				break;
			}
		}

		display.ClearRenderTarget(ivec3(0, 0, 0));

		const auto drawBegin = chrono::high_resolution_clock::now();
		for (Primitive* prim : objects)
		{
			model = rotate(model, 0.025f, vec3(0, 1, 0));
			mat4 mv = view * model;
			mat3 normal = { mv[0], mv[1], mv[2] };
			mat4 mvp = proj * mv;

			mat_ubo* ubo = static_cast<mat_ubo*> (prim->UBO());
			ubo->MVP = mvp;
			ubo->NORMAL = normal;

			rasterizer->Draw(prim);
		}
		const auto drawEnd = chrono::high_resolution_clock::now();
		//printf("Draw time: %lld\n", chrono::duration_cast<chrono::milliseconds> (drawEnd - drawBegin).count());

		display.UpdateColorBuffer(rasterizer->getFrameBuffer());

		const auto presentBegin = chrono::high_resolution_clock::now();
		display.Present();
		const auto presentEnd = chrono::high_resolution_clock::now();

		printf("Frame time: %lld\n", chrono::duration_cast<chrono::milliseconds> (presentEnd - drawBegin).count());
	}
#endif

	for (Primitive* obj : objects)
		SAFE_DELETE(obj);

	SAFE_DELETE(rasterizer);

	return 0;
}

void ImportScene(vector<Primitive*>& objs, const string& filename)
{
	tinyobj::attrib_t attribs;
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;

	//TODO: Handle multiple objects in a single .obj

	string err;
	bool ret = tinyobj::LoadObj(&attribs, &shapes, &materials, &err, filename.c_str());
	if (ret)
	{
		const bool hasUV = !attribs.texcoords.empty();
		const bool hasNormals = !attribs.normals.empty();

		for (size_t i = 0; i < shapes.size(); i++)
		{
			const tinyobj::shape_t& shape = shapes[i];

			VertexBuffer vbo;
			IndexBuffer ibo;

			AttributeBuffer<3> normals;
			AttributeBuffer<2> uvs;

			const vector<tinyobj::index_t>& indexBuffer = shape.mesh.indices;

			Primitive* obj = new Primitive();
			objs.push_back(obj);

			for (size_t idx = 0; idx < indexBuffer.size(); idx++)
			{
				DBG_ASSERT(indexBuffer[idx].vertex_index != -1);
				const uint64_t vtxIndex = indexBuffer[idx].vertex_index;

				if (hasNormals)
				{
					DBG_ASSERT(indexBuffer[idx].normal_index != -1);
					normals.m_index.push_back(indexBuffer[idx].normal_index);
				}

				if (hasUV)
				{
					uvs.m_index.push_back(indexBuffer[idx].texcoord_index);
				}

				ibo.push_back(vtxIndex);
			}

			for (size_t f = 0; f < attribs.vertices.size(); f += 3) // 3 -> # of vertices in a triangle face
			{
				// has to have vertices
				const float p0 = attribs.vertices[f];
				const float p1 = attribs.vertices[f + 1];
				const float p2 = attribs.vertices[f + 2];
				vbo.push_back(vec3(p0, p1, p2));
			}

			if (hasNormals)
			{
				for (size_t i = 0; i < attribs.normals.size(); i += 3)
				{
					const float n0 = attribs.normals[i];
					const float n1 = attribs.normals[i + 1];
					const float n2 = attribs.normals[i + 2];
					normals.m_data.push_back(vec3(n0, n1, n2));
				}
			}

			if (hasUV)
			{
				for (size_t i = 0; i < attribs.texcoords.size(); i += 2)
				{
					const float uv0 = attribs.texcoords[i];
					const float uv1 = attribs.texcoords[i + 1];
					uvs.m_data.push_back(vec2(uv0, uv1));
				}
			}

			obj->setVertexBuffer(vbo);
			obj->setIndexBuffer(ibo);
			if (hasUV) obj->appendAttributeBuffer(uvs);
			if (hasNormals) obj->appendAttributeBuffer(normals);
		}
	}
}