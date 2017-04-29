#pragma once

#include "stdafx.h"

namespace softlit
{
	enum class CullMode
	{
		CULL_DISABLED, // Culling disabled
		CULL_BACK, // Cull back-facing prims
		CULL_FACE // Cull front-facing prims
	};

	enum class VertexWinding
	{
		CLOCKWISE,
		COUNTER_CLOCKWISE // Default
	};

	struct Viewport
	{
		uint32_t x;
		uint32_t y;

		uint32_t width;
		uint32_t height;
	};

	struct RasterizerSetup
	{
		CullMode cullMode = CullMode::CULL_DISABLED;
		VertexWinding vertexWinding = VertexWinding::CLOCKWISE;

		Viewport viewport = { 0u, 0u, 1024u, 768u };
	};
}