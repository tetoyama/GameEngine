
#include "NativeScriptHandle.h"
#include "ScriptWrapper.h"

#include <msclr/gcroot.h>
using namespace System;

class NativeScriptHandle::Impl {
public:
	msclr::gcroot<ScriptWrapper^> wrapper;

	Impl(const std::string& name){
		wrapper = gcnew ScriptWrapper(gcnew String(name.c_str()));
	}

	void OnStart(){
		wrapper->OnStart();
	}
	void OnUpdate(float dt){
		wrapper->OnUpdate(dt);
	}
	void OnFixedUpdate(float dt){
		wrapper->OnFixedUpdate(dt);
	}
	void OnDraw(){
		wrapper->OnDraw();
	}
	void OnStop(){
		wrapper->OnStop();
	}
};

NativeScriptHandle::NativeScriptHandle(const std::string& name)
	: impl(new Impl(name)){}

NativeScriptHandle::~NativeScriptHandle(){
	delete impl;
}

void NativeScriptHandle::OnStart(){
	impl->OnStart();
}
void NativeScriptHandle::OnUpdate(float dt){
	impl->OnUpdate(dt);
}
void NativeScriptHandle::OnFixedUpdate(float dt){
	impl->OnFixedUpdate(dt);
}
void NativeScriptHandle::OnDraw(){
	impl->OnDraw();
}
void NativeScriptHandle::OnStop(){
	impl->OnStop();
}
