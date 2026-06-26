#include <cassert>
#include <cmath>
#include <vector>

#include "Engine/Scene/System/Render/Animation/AnimationPoseEvaluator.h"

// このTestは手作りの非Import Fixtureだけを使い、modelData.cppとAssimp Libraryを
// リンクしない。Fixtureに必要な初期化契約だけをこのTranslation Unitで定義する。
void ModelData::Release(){}

aiNode::aiNode()
	: mName(),
	  mTransformation(),
	  mParent(nullptr),
	  mNumChildren(0),
	  mChildren(nullptr),
	  mNumMeshes(0),
	  mMeshes(nullptr),
	  mMetaData(nullptr){}

aiScene::aiScene()
	: mFlags(0),
	  mRootNode(nullptr),
	  mNumMeshes(0),
	  mMeshes(nullptr),
	  mNumMaterials(0),
	  mMaterials(nullptr),
	  mNumAnimations(0),
	  mAnimations(nullptr),
	  mNumTextures(0),
	  mTextures(nullptr),
	  mNumLights(0),
	  mLights(nullptr),
	  mNumCameras(0),
	  mCameras(nullptr),
	  mMetaData(nullptr),
	  mName(),
	  mNumSkeletons(0),
	  mSkeletons(nullptr),
	  mPrivate(nullptr){}

namespace {

aiAnimation* MakePositionAnimation(const char* nodeName, float positionX){
	auto* animation = new aiAnimation();
	animation->mDuration = 1.0;
	animation->mTicksPerSecond = 60.0;
	animation->mNumChannels = 1;
	animation->mChannels = new aiNodeAnim*[1];

	auto* channel = new aiNodeAnim();
	channel->mNodeName = aiString(nodeName);
	channel->mNumPositionKeys = 1;
	channel->mPositionKeys = new aiVectorKey[1];
	channel->mPositionKeys[0].mTime = 0.0;
	channel->mPositionKeys[0].mValue = aiVector3D(positionX, 0.0f, 0.0f);
	channel->mNumRotationKeys = 1;
	channel->mRotationKeys = new aiQuatKey[1];
	channel->mRotationKeys[0].mTime = 0.0;
	channel->mRotationKeys[0].mValue = aiQuaternion();

	animation->mChannels[0] = channel;
	return animation;
}

void AddAnimation(
	ModelData& model,
	const char* name,
	aiAnimation* animation
){
	AnimationData data;
	data.Animation = animation;
	data.isImported = false;
	model.m_Animation.emplace(name, data);
}

} // namespace

int main(){
	ModelData model;
	auto* scene = new aiScene();
	auto* root = new aiNode();
	root->mName = aiString("Root");
	scene->mRootNode = root;

	model.AiScene = scene;
	model.enableRootMotion = true;
	model.m_BoneIndexMap.emplace("Root", 0u);
	model.m_Bones.resize(1);
	AddAnimation(model, "EntityA", MakePositionAnimation("Root", 1.0f));
	AddAnimation(model, "EntityB", MakePositionAnimation("Root", 3.0f));

	const float sharedMatrixBefore = model.m_Bones[0].Matrix.a4;
	const float sharedAnimationBefore = model.m_Bones[0].AnimationMatrix.a4;

	std::vector<BONE> entityAPose;
	std::vector<BONE> entityBPose;
	const std::vector<AnimationBlend> entityABlend{
		AnimationBlend{"EntityA", 1.0f, 0.0f, true}
	};
	const std::vector<AnimationBlend> entityBBlend{
		AnimationBlend{"EntityB", 1.0f, 0.0f, true}
	};

	assert(AnimationPoseEvaluator::Evaluate(
		model,
		entityABlend,
		0.0f,
		entityAPose
	));
	assert(AnimationPoseEvaluator::Evaluate(
		model,
		entityBBlend,
		0.0f,
		entityBPose
	));
	assert(entityAPose.size() == 1);
	assert(entityBPose.size() == 1);

	// 同じ共有ModelDataから、Entityごとに異なる姿勢が得られる。
	assert(std::fabs(entityAPose[0].Matrix.a4 - entityBPose[0].Matrix.a4) > 0.5f);

	// Evaluatorは共有Resource側のBone状態を変更しない。
	assert(model.m_Bones[0].Matrix.a4 == sharedMatrixBefore);
	assert(model.m_Bones[0].AnimationMatrix.a4 == sharedAnimationBefore);

	// 一方のEntity固有出力を変更しても、もう一方と共有Resourceへ伝播しない。
	const float entityBValue = entityBPose[0].Matrix.a4;
	entityAPose[0].Matrix.a4 = 99.0f;
	assert(entityBPose[0].Matrix.a4 == entityBValue);
	assert(model.m_Bones[0].Matrix.a4 == sharedMatrixBefore);

	// FixtureはModelDataの空Release後にProcess終了するため、手作りAssimp Nodeの
	// 個別解放はこの最小契約Testでは行わない。
	model.AiScene = nullptr;
	return 0;
}
