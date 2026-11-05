#pragma once

#include <JuceHeader.h>
#include "PresetManager.h"

// Client du « compte epices » : appairage par code, connexion, synchro des presets.
// Les requêtes tournent sur un thread de fond ; les callbacks reviennent sur le thread message.
class CloudClient
{
public:
    using Callback = std::function<void (bool ok, juce::String message)>;

    explicit CloudClient (PresetManager&);
    ~CloudClient();

    bool isLoggedIn() const { return token().isNotEmpty(); }
    juce::String email() const { return props->getValue ("email"); }
    juce::String server() const { return props->getValue ("server", "http://localhost:8787"); }
    void setServer (const juce::String& s) { props->setValue ("server", s.trim().trimCharactersAtEnd ("/")); props->saveIfNeeded(); }

    void claimCode (const juce::String& code, Callback);
    void login (const juce::String& email, const juce::String& password, Callback);
    void logout();
    void sync (Callback);
    void upload (const juce::var& preset, Callback);

private:
    PresetManager& presets;
    std::unique_ptr<juce::PropertiesFile> props;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    juce::ThreadPool pool { 1 };

    juce::String token() const { return props->getValue ("token"); }
    void storeSession (const juce::var& r);

    struct Response { int status = 0; juce::var json; juce::String error; };
    static Response request (const juce::String& url, const juce::String& method, const juce::String& token, const juce::String& body);
    void run (std::function<void()> job);
    void done (Callback cb, bool ok, juce::String msg);
};
