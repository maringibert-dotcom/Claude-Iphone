#include "PresetManager.h"
#include "BinaryData.h"

using namespace masala;

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

juce::File PresetManager::rootDir()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("epices").getChildFile ("Masala").getChildFile ("Presets");
}

juce::String PresetManager::makeId()
{
    return "user-" + juce::String::toHexString (juce::Time::currentTimeMillis()) + juce::String::toHexString (juce::Random::getSystemRandom().nextInt (65536));
}

bool PresetManager::writeJson (const juce::File& f, const juce::var& v)
{
    f.getParentDirectory().createDirectory();
    return f.replaceWithText (juce::JSON::toString (v, false));
}

void PresetManager::scanFolder (const juce::File& dir, PresetInfo::Source src)
{
    if (! dir.isDirectory()) return;
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.masala;*.json");
    files.sort();
    for (auto& f : files)
    {
        auto v = juce::JSON::parse (f);
        if (! v.isObject() || ! v["params"].isObject()) continue;
        PresetInfo p;
        p.json = v;
        p.id = v["id"].toString();
        if (p.id.isEmpty()) p.id = f.getFileNameWithoutExtension();
        p.name = v["name"].toString();
        p.category = v["category"].toString();
        p.file = f;
        p.source = src;
        presets.push_back (p);
    }
}

void PresetManager::rescan()
{
    presets.clear();
    auto bank = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::factory_json, BinaryData::factory_jsonSize));
    if (auto* arr = bank["presets"].getArray())
        for (auto& v : *arr)
        {
            PresetInfo p;
            p.json = v;
            p.id = v["id"].toString();
            p.name = v["name"].toString();
            p.category = v["category"].toString();
            p.source = PresetInfo::Source::Factory;
            presets.push_back (p);
        }
    scanFolder (userDir(), PresetInfo::Source::User);
    scanFolder (cloudDir(), PresetInfo::Source::Cloud);
    sendChangeMessage();
}

void PresetManager::loadInitial()
{
    rescan();
    for (int i = 0; i < (int) presets.size(); i++)
        if (presets[(size_t) i].name == "Harissa Wobble") { load (i); return; }
    if (! presets.empty()) load (0);
}

int PresetManager::currentIndex() const
{
    for (int i = 0; i < (int) presets.size(); i++)
        if (presets[(size_t) i].id == id) return i;
    return -1;
}

bool PresetManager::load (int index)
{
    if (index < 0 || index >= (int) presets.size()) return false;
    return loadJson (presets[(size_t) index].json);
}

void PresetManager::step (int dir)
{
    if (presets.empty()) return;
    const int n = (int) presets.size();
    load (((currentIndex() + dir) % n + n) % n);
}

bool PresetManager::loadJson (const juce::var& pr)
{
    const auto params = pr["params"];
    if (! params.isObject()) return false;
    for (int i = 0; i < PID::COUNT; i++)
    {
        const auto& d = kParams[i];
        auto* param = apvts.getParameter (d.id);
        if (param == nullptr) continue;
        float v = d.def;
        const auto pv = params[juce::Identifier (d.id)];
        if (d.type == ParamType::Choice && pv.isString())
        {
            const int idx = juce::StringArray::fromTokens (d.choices, "|", "").indexOf (pv.toString());
            if (idx >= 0) v = (float) idx;
        }
        else if (pv.isDouble() || pv.isInt() || pv.isInt64() || pv.isBool())
            v = (float) (double) pv;
        v = juce::jlimit (d.min, d.max, v);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
    }
    id = pr["id"].toString();
    name = pr["name"].toString();
    category = pr["category"].toString();
    for (int m = 0; m < 4; m++) macros[m] = "Macro " + juce::String (m + 1);
    if (auto* arr = pr["macros"].getArray())
        for (int m = 0; m < juce::jmin (4, arr->size()); m++)
            if (arr->getReference (m).toString().isNotEmpty()) macros[m] = arr->getReference (m).toString();
    sendChangeMessage();
    return true;
}

juce::var PresetManager::serialize (const juce::String& newId, const juce::String& newName, const juce::String& cat) const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("format", "epices.masala.preset");
    obj->setProperty ("version", 1);
    obj->setProperty ("id", newId);
    obj->setProperty ("name", newName);
    obj->setProperty ("category", cat);
    obj->setProperty ("author", "plugin");
    obj->setProperty ("tags", juce::Array<juce::var>());
    juce::Array<juce::var> m;
    for (auto& s : macros) m.add (s);
    obj->setProperty ("macros", m);
    auto* params = new juce::DynamicObject();
    for (int i = 0; i < PID::COUNT; i++)
    {
        const auto& d = kParams[i];
        const float v = apvts.getRawParameterValue (d.id)->load();
        if (d.type == ParamType::Choice)
            params->setProperty (d.id, juce::StringArray::fromTokens (d.choices, "|", "")[(int) v]);
        else
            params->setProperty (d.id, (double) v);
    }
    obj->setProperty ("params", juce::var (params));
    obj->setProperty ("updatedAt", (juce::int64) juce::Time::currentTimeMillis());
    return juce::var (obj);
}

PresetInfo PresetManager::saveUser (const juce::String& newName, const juce::String& cat, bool toCloudFolder)
{
    // même nom qu'un preset utilisateur existant → on l'écrase
    juce::String newId = makeId();
    for (auto& p : presets)
        if (p.source != PresetInfo::Source::Factory && p.name == newName) newId = p.id;
    const auto v = serialize (newId, newName, cat);
    const auto dir = toCloudFolder ? cloudDir() : userDir();
    const auto f = dir.getChildFile (newId + ".masala");
    writeJson (f, v);
    rescan();
    loadJson (v);
    PresetInfo info;
    info.id = newId; info.name = newName; info.category = cat; info.json = v; info.file = f;
    info.source = toCloudFolder ? PresetInfo::Source::Cloud : PresetInfo::Source::User;
    return info;
}

bool PresetManager::importFile (const juce::File& f)
{
    auto v = juce::JSON::parse (f);
    juce::Array<juce::var> list;
    if (auto* arr = v["presets"].getArray()) list = *arr;
    else list.add (v);
    int n = 0;
    for (auto& p : list)
    {
        if (! p["params"].isObject() || p["name"].toString().isEmpty()) continue;
        if (auto* o = p.getDynamicObject())
        {
            auto pid = p["id"].toString();
            if (pid.isEmpty() || pid.startsWith ("factory-")) { pid = makeId(); o->setProperty ("id", pid); }
            writeJson (userDir().getChildFile (pid + ".masala"), p);
            n++;
        }
    }
    rescan();
    return n > 0;
}

void PresetManager::writeMeta (juce::ValueTree& state) const
{
    state.setProperty ("presetId", id, nullptr);
    state.setProperty ("presetName", name, nullptr);
    state.setProperty ("presetCategory", category, nullptr);
    for (int m = 0; m < 4; m++) state.setProperty ("macro" + juce::String (m + 1) + "Name", macros[m], nullptr);
}

void PresetManager::readMeta (const juce::ValueTree& state)
{
    id = state.getProperty ("presetId", id).toString();
    name = state.getProperty ("presetName", name).toString();
    category = state.getProperty ("presetCategory", category).toString();
    for (int m = 0; m < 4; m++) macros[m] = state.getProperty ("macro" + juce::String (m + 1) + "Name", macros[m]).toString();
    sendChangeMessage();
}
