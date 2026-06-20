from pathlib import Path

path = Path("Source/GameApplication/Engine/Scene/Registry/componentRegistry.h")
text = path.read_text(encoding="utf-8-sig")

include_anchor = '#include "Interface/IComponentStorage.h"\n'
include_replacement = (
    '#include "Interface/IComponentStorage.h"\n'
    '#include "Query/ComponentQueryView.h"\n'
)
if text.count(include_anchor) != 1:
    raise RuntimeError("IComponentStorage include anchor mismatch")
text = text.replace(include_anchor, include_replacement, 1)

legacy_start = text.find("struct ComponentCollectionEntry {")
legacy_end_marker = "\nclass ComponentType {"
legacy_end = text.find(legacy_end_marker, legacy_start)
if legacy_start < 0 or legacy_end < 0:
    raise RuntimeError("legacy ComponentCollection block not found")
text = text[:legacy_start] + "class ComponentType {" + text[legacy_end + len(legacy_end_marker):]

old_method = '''\tComponentCollection GetAllComponentsOfEntitySorted(Entity entity){
\t\tComponentCollection::Container entries;
\t\tfor(ComponentView view : GetAllComponentViewsOfEntitySorted(entity)){
\t\t\tIComponent* legacy = nullptr;
\t\t\tauto typeIterator = m_idToTypeIndex.find(view.typeID);
\t\t\tif(typeIterator != m_idToTypeIndex.end()){
\t\t\t\tauto adapterIterator =
\t\t\t\t\tm_polymorphicAdapters.find(typeIterator->second);
\t\t\t\tif(adapterIterator != m_polymorphicAdapters.end()){
\t\t\t\t\tlegacy = adapterIterator->second(view.data);
\t\t\t\t}
\t\t\t}
\t\t\tentries.push_back({view, legacy});
\t\t}
\t\treturn ComponentCollection(std::move(entries));
\t}
'''
new_method = '''\tstd::vector<ComponentView> GetAllComponentsOfEntitySorted(Entity entity){
\t\treturn GetAllComponentViewsOfEntitySorted(entity);
\t}
'''
if text.count(old_method) != 1:
    raise RuntimeError("legacy sorted component method mismatch")
text = text.replace(old_method, new_method, 1)

query_anchor = '''\ttemplate<typename... Components>
\tstd::vector<Entity> QueryEntities(){
'''
query_methods = '''\ttemplate<typename... Accesses>
\tECSQuery::ComponentQueryView<Accesses...> Query() const {
\t\tComponentMask required;
\t\t(required.set(
\t\t\tComponentType::Get<typename ECSQuery::ComponentType<Accesses>>()
\t\t), ...);

\t\treturn ECSQuery::ComponentQueryView<Accesses...>(
\t\t\tm_entityManager->GetAllAlive(),
\t\t\tm_entityMasks,
\t\t\trequired
\t\t);
\t}

\ttemplate<typename... Components>
\tauto ReadQuery() const {
\t\treturn Query<ECSQuery::Read<Components>...>();
\t}

\ttemplate<typename... Components>
\tstd::vector<Entity> QueryEntities(){
'''
if text.count(query_anchor) != 1:
    raise RuntimeError("QueryEntities anchor mismatch")
text = text.replace(query_anchor, query_methods, 1)

path.write_text(text, encoding="utf-8")
