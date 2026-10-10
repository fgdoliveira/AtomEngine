#pragma once

// CPU profiling zones (v0.0.15, M95-M97; ADR-010).
//
//   ATOM_PROFILE_SCOPE("Collision");   // a zone: from here to the end of the block
//   ATOM_PROFILE_FRAME();              // the end of a frame
//
// With -DATOM_TRACE=ON they're recorded by the built-in tracer
// (Core/Trace.h) when ATOM_TRACE=<file prefix> is set; otherwise they
// compile to nothing. Include this from .cpp files only, so no profiler
// type reaches a public header. The names must be string literals.
//
// Several zones may open in one block: each gets its own variable (named
// by its line), and a later one, destroyed first, nests inside an earlier
// one until the block ends. A zone that must end early needs its own block.
#define ATOM_PROFILE_CONCAT_(a, b) a##b
#define ATOM_PROFILE_CONCAT(a, b) ATOM_PROFILE_CONCAT_(a, b)

#if ATOM_PROFILE_TRACE
#include "Core/Trace.h"
#define ATOM_PROFILE_SCOPE(name) ::Atom::Trace::Scope ATOM_PROFILE_CONCAT(atomProfileZone, __LINE__)(name)
#define ATOM_PROFILE_FRAME() ::Atom::Trace::FrameMark()
#define ATOM_PROFILE_CONNECTED() ::Atom::Trace::IsRecording()
#define ATOM_PROFILE_NAME "trace"
#else
#define ATOM_PROFILE_SCOPE(name) ((void)0)
#define ATOM_PROFILE_FRAME() ((void)0)
#define ATOM_PROFILE_CONNECTED() false
#define ATOM_PROFILE_NAME "none"
#endif
