#include "CloudClient.h"

CloudClient::CloudClient (PresetManager& p) : presets (p)
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Masala";
    o.folderName = "epices";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    props = std::make_unique<juce::PropertiesFile> (o);
}

CloudClient::~CloudClient()
{
    alive->store (false);
    pool.removeAllJobs (true, 5000);
}

void CloudClient::run (std::function<void()> job) { pool.addJob (std::move (job)); }

void CloudClient::done (Callback cb, bool ok, juce::String msg)
{
    auto flag = alive;
    juce::MessageManager::callAsync ([flag, cb, ok, msg] { if (flag->load() && cb) cb (ok, msg); });
}

CloudClient::Response CloudClient::request (const juce::String& url, const juce::String& method, const juce::String& tok, const juce::String& body)
{
    Response r;
    juce::URL u (url);
    if (body.isNotEmpty()) u = u.withPOSTData (body);
    juce::String headers = "Content-Type: application/json\r\n";
    if (tok.isNotEmpty()) headers << "Authorization: Bearer " << tok << "\r\n";
    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                    .withExtraHeaders (headers)
                    .withConnectionTimeoutMs (10000)
                    .withHttpRequestCmd (method)
                    .withStatusCode (&r.status);
    auto stream = u.createInputStream (opts);
    if (stream == nullptr) { r.error = "Serveur injoignable"; return r; }
    const auto text = stream->readEntireStreamAsString();
    r.json = juce::JSON::parse (text);
    if (r.status >= 400) r.error = r.json["error"].toString().isNotEmpty() ? r.json["error"].toString() : "Erreur " + juce::String (r.status);
    return r;
}

void CloudClient::storeSession (const juce::var& r)
{
    props->setValue ("token", r["token"].toString());
    props->setValue ("email", r["email"].toString());
    props->saveIfNeeded();
}

void CloudClient::claimCode (const juce::String& code, Callback cb)
{
    const auto url = server() + "/api/pair/claim";
    auto* o = new juce::DynamicObject();
    o->setProperty ("code", code.retainCharacters ("0123456789"));
    const auto body = juce::JSON::toString (juce::var (o));
    run ([this, url, body, cb] {
        auto r = request (url, "POST", {}, body);
        if (r.error.isNotEmpty()) return done (cb, false, r.error);
        juce::MessageManager::callAsync ([this, r, flag = alive] { if (flag->load()) storeSession (r.json); });
        done (cb, true, "Plugin relié à " + r.json["email"].toString());
    });
}

void CloudClient::login (const juce::String& mail, const juce::String& password, Callback cb)
{
    const auto url = server() + "/api/login";
    auto* o = new juce::DynamicObject();
    o->setProperty ("email", mail);
    o->setProperty ("password", password);
    const auto body = juce::JSON::toString (juce::var (o));
    run ([this, url, body, cb] {
        auto r = request (url, "POST", {}, body);
        if (r.error.isNotEmpty()) return done (cb, false, r.error);
        juce::MessageManager::callAsync ([this, r, flag = alive] { if (flag->load()) storeSession (r.json); });
        done (cb, true, "Connecté : " + r.json["email"].toString());
    });
}

void CloudClient::logout()
{
    props->removeValue ("token");
    props->removeValue ("email");
    props->saveIfNeeded();
}

void CloudClient::upload (const juce::var& preset, Callback cb)
{
    if (! isLoggedIn()) return done (cb, false, "Non connecté");
    const auto url = server() + "/api/presets/" + juce::URL::addEscapeChars (preset["id"].toString(), false);
    const auto body = juce::JSON::toString (preset, true);
    const auto tok = token();
    run ([this, url, body, tok, cb] {
        auto r = request (url, "PUT", tok, body);
        done (cb, r.error.isEmpty(), r.error.isEmpty() ? "Envoyé sur ton compte" : r.error);
    });
}

void CloudClient::sync (Callback cb)
{
    if (! isLoggedIn()) return done (cb, false, "Non connecté");
    const auto base = server();
    const auto tok = token();
    // presets créés dans le plugin hors-ligne (dossier User) à envoyer
    juce::Array<juce::var> localUser;
    for (auto& p : presets.all())
        if (p.source == PresetInfo::Source::User) localUser.add (p.json);

    run ([this, base, tok, localUser, cb] {
        auto r = request (base + "/api/presets", "GET", tok, {});
        if (r.error.isNotEmpty())
        {
            if (r.status == 401) juce::MessageManager::callAsync ([this, flag = alive] { if (flag->load()) logout(); });
            return done (cb, false, r.error);
        }
        juce::StringArray remoteIds;
        int pulled = 0, pushed = 0;
        const auto dir = PresetManager::cloudDir();
        dir.createDirectory();
        if (auto* arr = r.json["presets"].getArray())
            for (auto& p : *arr)
            {
                const auto id = p["id"].toString().retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_");
                if (id.isEmpty()) continue;
                remoteIds.add (id);
                const auto f = dir.getChildFile (id + ".masala");
                const auto local = juce::JSON::parse (f);
                if (! f.existsAsFile() || (juce::int64) local["updatedAt"] < (juce::int64) p["updatedAt"])
                {
                    PresetManager::writeJson (f, p);
                    pulled++;
                }
                else if ((juce::int64) local["updatedAt"] > (juce::int64) p["updatedAt"])
                {
                    if (request (base + "/api/presets/" + id, "PUT", tok, juce::JSON::toString (local, true)).error.isEmpty()) pushed++;
                }
            }
        // supprimés côté compte → supprimés localement
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.masala"))
            if (! remoteIds.contains (f.getFileNameWithoutExtension())) f.deleteFile();
        // presets du dossier User jamais envoyés
        for (auto& p : localUser)
        {
            const auto id = p["id"].toString();
            if (remoteIds.contains (id)) continue;
            if (request (base + "/api/presets/" + juce::URL::addEscapeChars (id, false), "PUT", tok, juce::JSON::toString (p, true)).error.isEmpty())
            {
                PresetManager::userDir().getChildFile (id + ".masala").moveFileTo (dir.getChildFile (id + ".masala"));
                pushed++;
            }
        }
        juce::MessageManager::callAsync ([this, flag = alive] { if (flag->load()) presets.rescan(); });
        done (cb, true, "Synchro OK : " + juce::String (pulled) + " reçu(s), " + juce::String (pushed) + " envoyé(s)");
    });
}
