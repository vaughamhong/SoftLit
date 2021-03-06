#include "stdafx.h"

#include "Primitive.h"

using namespace std;
using namespace glm;
using namespace softlit;

Primitive::Primitive(const PrimitiveSetup& setup)
	: m_setup(setup)
{
}

Primitive::~Primitive()
{
	m_vertexBuffer.clear();
	m_indexBuffer.clear();
}