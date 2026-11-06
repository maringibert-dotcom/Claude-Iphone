#include "PluginEditor.h"

using namespace masala;

//==============================================================================
// Look & feel epices audio
epices::LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, encre);
    setColour (juce::Slider::textBoxTextColourId, papier);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::trackColourId, orange);
    setColour (juce::Slider::backgroundColourId, noir3);
    setColour (juce::Slider::thumbColourId, noir); // remplissage de la cache
    setColour (juce::Label::textColourId, sable);
    setColour (juce::ComboBox::backgroundColourId, encre);
    setColour (juce::ComboBox::textColourId, papier);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::arrowColourId, sable);
    setColour (juce::PopupMenu::backgroundColourId, noir);
    setColour (juce::PopupMenu::textColourId, papier);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, orange);
    setColour (juce::PopupMenu::highlightedTextColourId, encre);
    setColour (juce::PopupMenu::headerTextColourId, orange);
    setColour (juce::TextButton::buttonColourId, noir2);
    setColour (juce::TextButton::textColourOffId, papier);
    setColour (juce::TextButton::textColourOnId, encre);
    setColour (juce::ToggleButton::textColourId, sable);
    setColour (juce::ToggleButton::tickColourId, orange);
    setColour (juce::AlertWindow::backgroundColourId, noir);
    setColour (juce::AlertWindow::textColourId, papier);
    setColour (juce::AlertWindow::outlineColourId, line);
    setColour (juce::TextEditor::backgroundColourId, encre);
    setColour (juce::TextEditor::textColourId, papier);
    setColour (juce::TextEditor::outlineColourId, line);
    setColour (juce::TextEditor::focusedOutlineColourId, orange);
    setColour (juce::TabbedComponent::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
}

void epices::LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::trackColourId);
    const float size = (float) juce::jmin (w, h) - 6.0f;
    const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = size * 0.5f;
    const float stroke = juce::jmax (3.0f, r * 0.16f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - stroke * 0.5f, r - stroke * 0.5f, 0, a0, a1, true);
    g.setColour (s.findColour (juce::Slider::backgroundColourId));
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float zero = bipolar ? a0 + (a1 - a0) * (float) s.valueToProportionOfLength (0.0) : a0;
    const float ang = a0 + pos * (a1 - a0);
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, r - stroke * 0.5f, r - stroke * 0.5f, 0, juce::jmin (zero, ang), juce::jmax (zero, ang), true);
    g.setColour (accent);
    g.strokePath (arc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // cache = anneau du symbole epices, le point indique la valeur
    const float cr = r * 0.56f;
    g.setColour (s.findColour (juce::Slider::thumbColourId));
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
    g.setColour (accent);
    g.drawEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2, stroke);
    const float dr = juce::jmax (2.5f, cr * 0.24f), dd = cr * 0.5f;
    g.fillEllipse (c.x + dd * std::sin (ang) - dr, c.y - dd * std::cos (ang) - dr, dr * 2, dr * 2);
}

// symbole epices : pilule + deux anneaux (repris des SVG officiels, viewBox 232×132)
static void drawEpicesSymbol (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour body, juce::Colour rings)
{
    const float s = juce::jmin (area.getWidth() / 220.0f, area.getHeight() / 120.0f);
    const float x0 = area.getCentreX() - 110 * s, y0 = area.getCentreY() - 60 * s;
    g.setColour (body);
    g.fillRoundedRectangle (x0, y0, 220 * s, 120 * s, 44 * s);
    g.setColour (rings);
    for (float cx : { 65.0f, 155.0f })
        g.drawEllipse (x0 + (cx - 34) * s, y0 + 26 * s, 68 * s, 68 * s, 15 * s);
    g.fillEllipse (x0 + (155 - 10.5f) * s, y0 + (60 - 10.5f) * s, 21 * s, 21 * s);
}

void epices::LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour (on ? orange : (down ? noir3 : (over ? noir3.withAlpha (0.8f) : noir2)));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f); // pilule, comme le symbole
}

void epices::LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);
    auto r = b.getLocalBounds().toFloat();
    auto sw = juce::Rectangle<float> (0, 0, 38, 22).withCentre ({ r.getCentreX(), r.getY() + 16 });
    const bool on = b.getToggleState();
    g.setColour (on ? accent.withAlpha (0.25f) : encre);
    g.fillRoundedRectangle (sw, 11);
    g.setColour (on ? accent : line);
    g.drawRoundedRectangle (sw, 11, 1);
    g.setColour (on ? accent : muted);
    g.fillEllipse (on ? sw.getRight() - 19 : sw.getX() + 3, sw.getY() + 3, 16, 16);
    g.setColour (sable);
    g.setFont (juce::FontOptions (10.5f));
    g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().withTrimmedTop (32), juce::Justification::centredTop, 2);
}

void epices::LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (encre);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    if (box.hasKeyboardFocus (false)) { g.setColour (orange); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.5f); }
    juce::Path arrow;
    arrow.addTriangle ((float) w - 18, (float) h * 0.42f, (float) w - 8, (float) h * 0.42f, (float) w - 13, (float) h * 0.62f);
    g.setColour (sable);
    g.fillPath (arrow);
}

void epices::LookAndFeel::drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool over, bool)
{
    const bool front = b.isFrontTab();
    auto r = b.getActiveArea().toFloat().reduced (3, 4);
    if (front)
    {
        g.setColour (orange);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
    else if (over)
    {
        g.setColour (noir2);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
    g.setColour (front ? encre : sable);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText().toUpperCase(), r, juce::Justification::centred);
}

//==============================================================================
void WaveView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (epices::encre);
    g.fillRoundedRectangle (r, 12);

    auto& ap = proc.apvts;
    const juce::String pre = isA ? "oscA." : "oscB.";
    const int table = (int) ap.getRawParameterValue (pre + "table")->load();
    const int wm = (int) ap.getRawParameterValue (pre + "warpMode")->load();
    float pos = ap.getRawParameterValue (pre + "pos")->load();
    float warp = ap.getRawParameterValue (pre + "warp")->load();
    if (proc.liveModValid)
    {
        pos += proc.liveMod[(size_t) (isA ? Dest::OscAPos : Dest::OscBPos)].load();
        warp += proc.liveMod[(size_t) (isA ? Dest::OscAWarp : Dest::OscBWarp)].load();
    }
    pos = juce::jlimit (0.0f, 1.0f, pos);
    warp = juce::jlimit (0.0f, 1.0f, warp);
    const auto& wt = Wavetables::get();
    const int N = 140;
    const float w = r.getWidth() - 60, h = r.getHeight();
    auto drawFrame = [&] (float fpos, float ox, float oy, juce::Colour col, float thick) {
        const auto d = wt.preview (table, fpos, wm, warp, N);
        juce::Path p;
        for (int i = 0; i < N; i++)
        {
            const float x = r.getX() + 12 + ox + (float) i / (N - 1) * w;
            const float y = r.getY() + h * 0.58f + oy - d[(size_t) i] * h * 0.3f;
            i ? p.lineTo (x, y) : p.startNewSubPath (x, y);
        }
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    // frames de fond en brume : couches larges et très transparentes, estompées avec la profondeur
    for (int f = 7; f >= 0; f--)
    {
        const float depth = 1.0f - f / 9.0f;
        for (auto [lw, a] : { std::pair { 10.0f, 0.018f }, std::pair { 6.0f, 0.025f }, std::pair { 3.0f, 0.03f } })
            drawFrame (f / 7.0f, f * 5.0f, -f * 4.0f, epices::papier.withAlpha (a * depth), lw);
    }
    const auto col = isA ? epices::orange : epices::papier;
    drawFrame (pos, pos * 35, -pos * 28, col, 3.0f);
    g.setColour (epices::muted);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String (juce::StringArray::fromTokens (kParams[isA ? PID::oscA_table : PID::oscB_table].choices, "|", "")[table]).toUpperCase()
                    + "  " + juce::String (juce::roundToInt (pos * 100)) + "%",
                r.reduced (10, 6), juce::Justification::bottomLeft);
}

//==============================================================================
static const ParamDef* findDef (const juce::String& id)
{
    for (auto& d : kParams) if (id == d.id) return &d;
    return nullptr;
}

ParamGroup::ParamGroup (MasalaProcessor& p, juce::String t, juce::Colour a, juce::StringArray ids, juce::StringArray labels)
    : title (std::move (t)), accent (a)
{
    for (int i = 0; i < ids.size(); i++)
    {
        const auto* d = findDef (ids[i]);
        jassert (d != nullptr);
        if (d == nullptr) continue;
        Item it;
        const juce::String label = i < labels.size() && labels[i].isNotEmpty() ? labels[i] : juce::String (d->name);
        if (d->type == ParamType::Choice)
        {
            auto cb = std::make_unique<juce::ComboBox>();
            cb->addItemList (juce::StringArray::fromTokens (d->choices, "|", ""), 1);
            it.ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, d->id, *cb);
            it.comp = std::move (cb);
            it.wide = true;
            it.label = std::make_unique<juce::Label> (juce::String(), label.toUpperCase());
        }
        else if (d->type == ParamType::Bool)
        {
            auto tb = std::make_unique<juce::ToggleButton> (label);
            tb->setColour (juce::ToggleButton::tickColourId, accent);
            it.ba = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, d->id, *tb);
            it.comp = std::move (tb);
        }
        else
        {
            auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
            s->setMouseDragSensitivity (400); // réglage plus fin (même ressenti que l'appli)
            s->setColour (juce::Slider::trackColourId, accent);
            s->setDoubleClickReturnValue (true, d->def);
            it.sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, d->id, *s);
            if (d->type == ParamType::Float) s->setNumDecimalPlacesToDisplay (d->max - d->min > 50 ? 0 : 2);
            it.comp = std::move (s);
            it.label = std::make_unique<juce::Label> (juce::String(), label.toUpperCase());
        }
        addAndMakeVisible (*it.comp);
        if (it.label)
        {
            it.label->setJustificationType (juce::Justification::centred);
            it.label->setFont (juce::FontOptions (10.5f));
            it.label->setColour (juce::Label::textColourId, epices::sable);
            addAndMakeVisible (*it.label);
        }
        items.push_back (std::move (it));
    }
}

static constexpr int kCellW = 74, kCellH = 92, kHead = 34, kPad = 10, kCols = 4;

int ParamGroup::preferredWidth() const { return kCols * kCellW + kPad * 2; }

void ParamGroup::resized()
{
    int y = kHead;
    if (top) { top->setBounds (kPad, y, getWidth() - kPad * 2, topH); y += topH + 6; }
    int col = 0;
    for (auto& it : items)
    {
        const int span = it.wide ? 2 : 1;
        if (col + span > kCols) { col = 0; y += kCellH; }
        auto cell = juce::Rectangle<int> (kPad + col * kCellW, y, kCellW * span, kCellH);
        if (it.wide)
        {
            it.label->setBounds (cell.removeFromTop (22).reduced (4, 0));
            it.comp->setBounds (cell.removeFromTop (28).reduced (6, 0));
        }
        else if (it.label)
        {
            it.label->setBounds (cell.removeFromBottom (16));
            it.comp->setBounds (cell.reduced (4, 2));
        }
        else it.comp->setBounds (cell.reduced (4, 8));
        col += span;
    }
}

void ParamGroup::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (epices::noir2);
    g.fillRoundedRectangle (r, 18);
    g.setColour (accent);
    g.fillEllipse ((float) kPad + 2, 15, 9, 9);
    g.setColour (epices::papier);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (title, kPad + 18, 8, getWidth() - 30, 22, juce::Justification::centredLeft);
}

void ParamPage::resized()
{
    // empilement en colonnes : chaque groupe prend la hauteur dont il a besoin
    const int gap = 10;
    int x = 0, y = 0, colW = 0;
    for (auto& g : groups)
    {
        const int w = g->preferredWidth();
        // estimation de la hauteur : on simule la mise en page
        g->setBounds (0, 0, w, 2000);
        int maxBottom = 0;
        for (auto* c : g->getChildren()) maxBottom = juce::jmax (maxBottom, c->getBottom());
        const int h = maxBottom + 12;
        if (y > 0 && y + h > getHeight()) { x += colW + gap; y = 0; colW = 0; }
        g->setBounds (x, y, w, h);
        y += h + gap;
        colW = juce::jmax (colW, w);
    }
}

//==============================================================================
MasalaEditor::MasalaEditor (MasalaProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (1040, 680);

    addAndMakeVisible (presetBtn);
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);
    addAndMakeVisible (saveBtn);
    addAndMakeVisible (accountBtn);
    addAndMakeVisible (status);
    status.setColour (juce::Label::textColourId, epices::muted);
    status.setJustificationType (juce::Justification::centredRight);
    status.setFont (juce::FontOptions (12.0f));
    presetBtn.onClick = [this] { showPresetMenu(); };
    prevBtn.onClick = [this] { proc.panic = true; proc.presets.step (-1); };
    nextBtn.onClick = [this] { proc.panic = true; proc.presets.step (1); };
    saveBtn.onClick = [this] { savePreset(); };
    accountBtn.onClick = [this] { showAccountMenu(); };

    const juce::Colour mc[4] = { epices::noir, epices::noir, epices::noir, epices::noir };
    for (int m = 0; m < 4; m++)
    {
        macro[m].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        macro[m].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        macro[m].setMouseDragSensitivity (400);
        // macros : comme le symbole « orange, anneaux noirs »
        macro[m].setColour (juce::Slider::trackColourId, mc[m]);
        macro[m].setColour (juce::Slider::thumbColourId, epices::orange);
        macro[m].setColour (juce::Slider::backgroundColourId, juce::Colour (0xffb83c10));
        macroAtt[m] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "macro" + juce::String (m + 1), macro[m]);
        addAndMakeVisible (macro[m]);
        macroLabel[m].setJustificationType (juce::Justification::centred);
        macroLabel[m].setFont (juce::FontOptions (13.0f, juce::Font::bold));
        macroLabel[m].setColour (juce::Label::textColourId, epices::noir);
        addAndMakeVisible (macroLabel[m]);
    }

    using G = ParamGroup;
    auto osc = std::make_unique<ParamPage>();
    for (bool a : { true, false })
    {
        const juce::String o = a ? "oscA." : "oscB.";
        auto g = std::make_unique<G> (proc, a ? "Osc A" : "Osc B", a ? epices::orange : epices::papier,
                                      juce::StringArray { o + "on", o + "table", o + "warpMode", o + "pos", o + "level", o + "semi", o + "fine", o + "unison", o + "detune", o + "spread", o + "warp" },
                                      juce::StringArray { "On", "Table", "Warp", "Position", "Niveau", "Demi-ton", "Fin", "Unisson", "Detune", "Stereo", "Warp" });
        auto wv = std::make_unique<WaveView> (proc, a);
        (a ? waveA : waveB) = wv.get();
        g->addTop (std::move (wv), 110);
        osc->add (std::move (g));
    }
    osc->add (std::make_unique<G> (proc, "Sub / Bruit / FM", epices::peche,
                                   juce::StringArray { "sub.on", "sub.oct", "sub.shape", "sub.level", "noise.level", "fm.amount" },
                                   juce::StringArray { "Sub", "Octave", "Forme", "Sub", "Bruit", "FM B>A" }));

    auto filt = std::make_unique<ParamPage>();
    filt->add (std::make_unique<G> (proc, "Filtre", epices::orange,
                                    juce::StringArray { "filter.on", "filter.type", "filter.cutoff", "filter.res", "filter.drive", "filter.env", "filter.keytrack" },
                                    juce::StringArray { "On", "Type", "Cutoff", "Reso", "Drive", "Env 2", "Key" }));
    filt->add (std::make_unique<G> (proc, "Env 1 - Amp", epices::orange, juce::StringArray { "env1.a", "env1.d", "env1.s", "env1.r" }, juce::StringArray { "Attaque", "Decay", "Sustain", "Release" }));
    filt->add (std::make_unique<G> (proc, "Env 2 - Mod", epices::papier, juce::StringArray { "env2.a", "env2.d", "env2.s", "env2.r" }, juce::StringArray { "Attaque", "Decay", "Sustain", "Release" }));

    auto modp = std::make_unique<ParamPage>();
    for (int l = 1; l <= 2; l++)
    {
        const juce::String pre = "lfo" + juce::String (l) + ".";
        modp->add (std::make_unique<G> (proc, "LFO " + juce::String (l), l == 1 ? epices::orange : epices::papier,
                                        juce::StringArray { pre + "shape", pre + "div", pre + "sync", pre + "retrig", pre + "rate" },
                                        juce::StringArray { "Forme", "Division", "Sync", "Retrig", "Vitesse Hz" }));
    }
    for (int half = 0; half < 2; half++)
    {
        juce::StringArray ids, labels;
        for (int s = 1 + half * 4; s <= 4 + half * 4; s++)
        {
            const juce::String pre = "mod" + juce::String (s) + ".";
            ids.addArray (juce::StringArray { pre + "src", pre + "dst" });
            labels.addArray (juce::StringArray { juce::String (s) + " Source", juce::String (s) + " Destination" });
        }
        modp->add (std::make_unique<G> (proc, half ? "Matrice 5-8" : "Matrice 1-4", epices::orange, ids, labels));
        juce::StringArray amts, amtL;
        for (int s = 1 + half * 4; s <= 4 + half * 4; s++) { amts.add ("mod" + juce::String (s) + ".amt"); amtL.add ("Qte " + juce::String (s)); }
        modp->add (std::make_unique<G> (proc, half ? "Quantites 5-8" : "Quantites 1-4", epices::orange, amts, amtL));
    }

    auto fx = std::make_unique<ParamPage>();
    fx->add (std::make_unique<G> (proc, "Distorsion", epices::orange, juce::StringArray { "fx.dist.on", "fx.dist.type", "fx.dist.drive", "fx.dist.mix" }, juce::StringArray { "On", "Type", "Drive", "Mix" }));
    fx->add (std::make_unique<G> (proc, "Squash", epices::orange, juce::StringArray { "fx.comp.on", "fx.comp.amount", "fx.comp.mix" }, juce::StringArray { "On", "Squash", "Mix" }));
    fx->add (std::make_unique<G> (proc, "Delay", epices::papier, juce::StringArray { "fx.delay.on", "fx.delay.div", "fx.delay.pingpong", "fx.delay.fb", "fx.delay.mix" }, juce::StringArray { "On", "Temps", "Ping-pong", "Feedback", "Mix" }));
    fx->add (std::make_unique<G> (proc, "Reverb", epices::papier, juce::StringArray { "fx.reverb.on", "fx.reverb.size", "fx.reverb.damp", "fx.reverb.mix" }, juce::StringArray { "On", "Taille", "Amorti", "Mix" }));

    auto play = std::make_unique<ParamPage>();
    play->add (std::make_unique<G> (proc, "Jeu", epices::orange, juce::StringArray { "global.legato", "global.poly", "global.glide", "global.bend", "global.volume" }, juce::StringArray { "Legato", "Voix", "Glide", "Bend", "Volume" }));

    const auto bg = juce::Colours::transparentBlack;
    tabs.addTab ("Osc", bg, osc.release(), true);
    tabs.addTab ("Filtre", bg, filt.release(), true);
    tabs.addTab ("Mod", bg, modp.release(), true);
    tabs.addTab ("FX", bg, fx.release(), true);
    tabs.addTab ("Jeu", bg, play.release(), true);
    tabs.setTabBarDepth (40);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);

    proc.presets.addChangeListener (this);
    refreshPreset();
    setStatus (proc.cloud.isLoggedIn() ? "Connecte : " + proc.cloud.email() : "Hors ligne - Compte > Code pour relier ton telephone");
    startTimerHz (30);
}

MasalaEditor::~MasalaEditor()
{
    proc.presets.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void MasalaEditor::paint (juce::Graphics& g)
{
    g.fillAll (epices::noir);
    drawEpicesSymbol (g, { 18, 18, 56, 32 }, epices::orange, epices::noir);
    g.setColour (epices::papier);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("masala", 84, 10, 140, 34, juce::Justification::centredLeft);
    g.setColour (epices::sable);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("P A R   e p i c e s   A U D I O", 86, 40, 200, 14, juce::Justification::centredLeft);

    // colonne des macros
    auto mr = juce::Rectangle<float> (16, 74, 150, (float) getHeight() - 90);
    g.setColour (epices::orange);
    g.fillRoundedRectangle (mr, 18);
    g.setColour (epices::noir);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("Macros", mr.removeFromTop (30), juce::Justification::centred);
}

void MasalaEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (66).reduced (16, 14);
    top.removeFromLeft (300);
    accountBtn.setBounds (top.removeFromRight (100));
    top.removeFromRight (8);
    saveBtn.setBounds (top.removeFromRight (110));
    top.removeFromRight (8);
    nextBtn.setBounds (top.removeFromRight (34));
    top.removeFromRight (4);
    presetBtn.setBounds (top.removeFromRight (290));
    top.removeFromRight (4);
    prevBtn.setBounds (top.removeFromRight (34));
    status.setBounds (getLocalBounds().removeFromBottom (24).reduced (16, 0).withTrimmedLeft (170));

    auto left = juce::Rectangle<int> (16, 104, 150, getHeight() - 124);
    const int each = left.getHeight() / 4;
    for (int m = 0; m < 4; m++)
    {
        auto cell = left.removeFromTop (each);
        macroLabel[m].setBounds (cell.removeFromBottom (24));
        macro[m].setBounds (cell.reduced (22, 4));
    }
    tabs.setBounds (r.withTrimmedLeft (180).withTrimmedRight (12).withTrimmedBottom (24));
}

void MasalaEditor::setStatus (const juce::String& s) { status.setText (s, juce::dontSendNotification); }

void MasalaEditor::refreshPreset()
{
    auto& pm = proc.presets;
    presetBtn.setButtonText (pm.currentCategory().toUpperCase() + "   " + pm.currentName());
    for (int m = 0; m < 4; m++) macroLabel[m].setText (pm.macroName (m), juce::dontSendNotification);
    accountBtn.setButtonText (proc.cloud.isLoggedIn() ? juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x8f Compte")) : "Compte");
}

void MasalaEditor::changeListenerCallback (juce::ChangeBroadcaster*) { refreshPreset(); }

void MasalaEditor::timerCallback()
{
    if (waveA && waveA->isShowing()) waveA->repaint();
    if (waveB && waveB->isShowing()) waveB->repaint();
}

void MasalaEditor::showPresetMenu()
{
    auto& list = proc.presets.all();
    juce::PopupMenu menu;
    std::map<juce::String, juce::PopupMenu> cats;
    juce::StringArray order;
    juce::PopupMenu mine, cloud;
    for (int i = 0; i < (int) list.size(); i++)
    {
        const auto& p = list[(size_t) i];
        const bool cur = p.id == proc.presets.currentId();
        if (p.source == PresetInfo::Source::Factory)
        {
            if (! order.contains (p.category)) order.add (p.category);
            cats[p.category].addItem (i + 1, p.name, true, cur);
        }
        else if (p.source == PresetInfo::Source::Cloud) cloud.addItem (i + 1, p.name + "  (" + p.category + ")", true, cur);
        else mine.addItem (i + 1, p.name + "  (" + p.category + ")", true, cur);
    }
    menu.addSectionHeader ("USINE");
    for (auto& c : order) menu.addSubMenu (c, cats[c]);
    menu.addSeparator();
    menu.addSectionHeader ("MES PRESETS");
    menu.addSubMenu ("Compte epices (cloud)", cloud, cloud.getNumItems() > 0);
    menu.addSubMenu ("Sur cet ordinateur", mine, mine.getNumItems() > 0);
    menu.addSeparator();
    menu.addItem (10001, "Importer un fichier .masala...");
    menu.addItem (10002, "Ouvrir le dossier des presets");
    menu.addItem (10003, "Rafraichir");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBtn), [this] (int r) {
        if (r <= 0) return;
        if (r == 10001)
        {
            chooser = std::make_unique<juce::FileChooser> ("Importer", juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.masala;*.json");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) {
                if (fc.getResult().existsAsFile())
                    setStatus (proc.presets.importFile (fc.getResult()) ? "Preset importe" : "Fichier illisible");
            });
        }
        else if (r == 10002) { PresetManager::rootDir().createDirectory(); PresetManager::rootDir().startAsProcess(); }
        else if (r == 10003) proc.presets.rescan();
        else { proc.panic = true; proc.presets.load (r - 1); }
    });
}

void MasalaEditor::savePreset()
{
    dialog = std::make_unique<juce::AlertWindow> ("Enregistrer le preset", proc.cloud.isLoggedIn() ? "Il sera aussi envoye sur ton compte (et donc sur ton telephone)." : "Connecte-toi pour le retrouver sur ton telephone.", juce::MessageBoxIconType::NoIcon);
    const bool isUser = ! proc.presets.currentId().startsWith ("factory-");
    dialog->addTextEditor ("name", isUser ? proc.presets.currentName() : proc.presets.currentName() + " (moi)", "Nom");
    juce::StringArray cats { "Dubstep", "Riddim", "Tribe", "Deep / Dub", "Halftime", "Leads & Pads", "FX", "Mes presets" };
    dialog->addComboBox ("cat", cats, "Categorie");
    if (auto* cb = dialog->getComboBoxComponent ("cat")) cb->setText (proc.presets.currentCategory().isNotEmpty() ? proc.presets.currentCategory() : "Mes presets");
    dialog->addButton ("Enregistrer", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Annuler", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result) {
        if (result != 1) return;
        const auto name = dialog->getTextEditorContents ("name").trim();
        const auto cat = dialog->getComboBoxComponent ("cat")->getText();
        if (name.isEmpty()) return;
        const bool cloudOn = proc.cloud.isLoggedIn();
        auto info = proc.presets.saveUser (name, cat, cloudOn);
        setStatus ("Enregistre : " + name);
        if (cloudOn)
            proc.cloud.upload (info.json, [this] (bool ok, juce::String msg) { setStatus (ok ? "Enregistre et synchronise" : "Enregistre localement (" + msg + ")"); });
    }), false);
}

void MasalaEditor::showAccountMenu()
{
    auto& c = proc.cloud;
    juce::PopupMenu m;
    if (c.isLoggedIn())
    {
        m.addSectionHeader (c.email());
        m.addItem (1, "Synchroniser maintenant");
        m.addItem (2, "Deconnexion");
    }
    else
    {
        m.addItem (3, "Entrer le code du telephone...");
        m.addItem (4, "Connexion par e-mail...");
    }
    m.addSeparator();
    m.addItem (5, "Serveur : " + c.server());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (accountBtn), [this] (int r) {
        auto& cl = proc.cloud;
        auto after = [this] (bool ok, juce::String msg) {
            setStatus (msg);
            refreshPreset();
            if (ok && proc.cloud.isLoggedIn())
                proc.cloud.sync ([this] (bool, juce::String m2) { setStatus (m2); refreshPreset(); });
        };
        if (r == 1) { setStatus ("Synchronisation..."); cl.sync ([this] (bool, juce::String msg) { setStatus (msg); }); }
        else if (r == 2) { cl.logout(); refreshPreset(); setStatus ("Deconnecte"); }
        else if (r == 3 || r == 4 || r == 5)
        {
            const juce::String title = r == 3 ? "Relier le plugin" : (r == 4 ? "Connexion" : "Serveur");
            const juce::String msg = r == 3 ? "Dans l'appli MASALA : Compte > Generer un code." : (r == 4 ? "Ton compte epices" : "Adresse du serveur epices");
            dialog = std::make_unique<juce::AlertWindow> (title, msg, juce::MessageBoxIconType::NoIcon);
            if (r == 3) dialog->addTextEditor ("code", "", "Code a 6 chiffres");
            if (r == 4) { dialog->addTextEditor ("email", cl.email(), "E-mail"); dialog->addTextEditor ("pass", "", "Mot de passe", true); }
            if (r == 5) dialog->addTextEditor ("server", cl.server(), "URL");
            dialog->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
            dialog->addButton ("Annuler", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            dialog->enterModalState (true, juce::ModalCallbackFunction::create ([this, r, after] (int res) {
                if (res != 1) return;
                auto& cc = proc.cloud;
                setStatus ("Connexion...");
                if (r == 3) cc.claimCode (dialog->getTextEditorContents ("code"), after);
                else if (r == 4) cc.login (dialog->getTextEditorContents ("email").trim(), dialog->getTextEditorContents ("pass"), after);
                else { cc.setServer (dialog->getTextEditorContents ("server")); setStatus ("Serveur : " + cc.server()); }
            }), false);
        }
    });
}
