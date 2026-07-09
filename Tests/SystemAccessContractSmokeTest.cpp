#include <cassert>

#include "Engine/Scene/System/Scheduler/SystemAccess.h"

struct ReadOnlyComponent {};
struct WriteComponent {};
struct RenderResource {};

int main(){
	SystemAccess none;
	assert(!IsStructuralAccessDeclared(none.structuralAccess));
	assert(!none.CanEmitStructuralCommands());
	assert(!none.CanWriteWorldStructureImmediately());
	assert(!none.RequiresStructuralIsolation());

	SystemAccess emit;
	emit.SetStructuralAccess(StructuralAccess::EmitCommands);
	assert(IsStructuralAccessDeclared(emit.structuralAccess));
	assert(emit.CanEmitStructuralCommands());
	assert(!emit.CanWriteWorldStructureImmediately());
	assert(!emit.RequiresStructuralIsolation());
	assert(!emit.ConflictsWith(none));

	SystemAccess exclusive;
	exclusive.SetStructuralAccess(StructuralAccess::ExclusiveWorldWrite);
	assert(IsStructuralAccessDeclared(exclusive.structuralAccess));
	assert(exclusive.CanEmitStructuralCommands());
	assert(exclusive.CanWriteWorldStructureImmediately());
	assert(exclusive.RequiresStructuralIsolation());
	assert(exclusive.ConflictsWith(none));
	assert(exclusive.ConflictsWith(emit));

	SystemAccess reader;
	reader.ReadComponent<ReadOnlyComponent>();
	SystemAccess writer;
	writer.WriteComponent<ReadOnlyComponent>();
	assert(reader.ConflictsWith(writer));
	assert(writer.ConflictsWith(reader));

	SystemAccess independentWriter;
	independentWriter.WriteComponent<WriteComponent>();
	independentWriter.WriteResource<RenderResource>();
	assert(!reader.ConflictsWith(independentWriter));

	SystemAccess legacy = SystemAccess::LegacyExclusive();
	assert(legacy.ConflictsWith(none));
	assert(legacy.ConflictsWith(reader));
	assert(legacy.ConflictsWith(exclusive));
	return 0;
}
