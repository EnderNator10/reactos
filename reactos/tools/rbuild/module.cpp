// module.cpp

#include "pch.h"
#include <assert.h>

#include "rbuild.h"

using std::string;
using std::vector;

string
FixSeparator ( const string& s )
{
	string s2(s);
	char* p = strchr ( &s2[0], CBAD_SEP );
	while ( p )
	{
		*p++ = CSEP;
		p = strchr ( p, CBAD_SEP );
	}
	return s2;
}

Module::Module ( Project* project,
                 const XMLElement& moduleNode,
                 const string& moduleName,
                 const string& modulePath )
	: project(project),
	  node(moduleNode),
	  name(moduleName),
	  path(modulePath)
{
	type = GetModuleType ( *moduleNode.GetAttribute ( "type", true ) );
}

Module::~Module ()
{
	size_t i;
	for ( i = 0; i < files.size(); i++ )
		delete files[i];
	for ( i = 0; i < libraries.size(); i++ )
		delete libraries[i];
}

void
Module::ProcessXML ( const XMLElement& e,
                     const string& path )
{
	string subpath ( path );
	if ( e.name == "file" && e.value.size () )
	{
		files.push_back ( new File ( path + CSEP + e.value ) );
	}
	else if ( e.name == "library" && e.value.size () )
	{
		libraries.push_back ( new Library ( e.value ) );
	}
	else if ( e.name == "directory" )
	{
		const XMLAttribute* att = e.GetAttribute ( "name", true );
		assert(att);
		subpath = path + CSEP + att->value;
	}
	else if ( e.name == "include" )
	{
		Include* include = new Include ( project, this, e );
		includes.push_back ( include );
		include->ProcessXML ( e );
	}
	else if ( e.name == "define" )
	{
		Define* define = new Define ( project, this, e );
		defines.push_back ( define );
		define->ProcessXML ( e );
	}
	for ( size_t i = 0; i < e.subElements.size (); i++ )
		ProcessXML ( *e.subElements[i], subpath );
}

ModuleType
Module::GetModuleType ( const XMLAttribute& attribute )
{
	if ( attribute.value == "buildtool" )
		return BuildTool;
	if ( attribute.value == "staticlibrary" )
		return StaticLibrary;
	if ( attribute.value == "kernelmodedll" )
		return KernelModeDLL;
	throw InvalidAttributeValueException ( attribute.name,
	                                       attribute.value );
}

string
Module::GetPath ()
{
	return FixSeparator (path) + CSEP + name + EXEPOSTFIX;
}


File::File ( const string& _name )
	: name(_name)
{
}


Library::Library ( const string& _name )
	: name(_name)
{
}
