#include "PluginEditor.h"

using namespace masala;

//==============================================================================
// Look & feel « épices »
epices::LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, poivre);
    setColour (juce::Slider::textBoxTextColourId, creme);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::trackColourId, safran);
    setColour (juce::Slider::backgroundColourId, cacao);
    setColour (juce::Slider::thumbColourId, creme);
    setColour (juce::Label::textColourId, sable);
    setColour (juce::ComboBox::backgroundColourId, poivre);
    setColour (juce::ComboBox::textColourId, creme);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::arrowColourId, sable);
    setColour (juce::PopupMenu::backgroundColourId, torref);
    setColour (juce::PopupMenu::textColourId, creme);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, safran);
    setColour (juce::PopupMenu::highlightedTextColourId, poivre);
    setColour (juce::PopupMenu::headerTextColourId, safran);
    setColour (juce::TextButton::buttonColourId, cannelle);
    setColour (juce::TextButton::textColourOffId, creme);
    setColour (juce::TextButton::textColourOnId, poivre);
    setColour (juce::ToggleButton::textColourId, sable);
    setColour (juce::ToggleButton::tickColourId, safran);
    setColour (juce::AlertWindow::backgroundColourId, torref);
    setColour (juce::AlertWindow::textColourId, creme);
    setColour (juce::AlertWindow::outlineColourId, line);
    setColour (juce::TextEditor::backgroundColourId, poivre);
    setColour (juce::TextEditor::textColourId, creme);
    setColour (juce::TextEditor::outlineColourId, line);
    setColour (juce::TextEditor::focusedOutlineColourId, safran);
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
    g.setColour (cacao);
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float zero = bipolar ? a0 + (a1 - a0) * (float) s.valueToProportionOfLength (0.0) : a0;
    const float ang = a0 + pos * (a1 - a0);
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, r - stroke * 0.5f, r - stroke * 0.5f, 0, juce::jmin (zero, ang), juce::jmax (zero, ang), true);
    g.setColour (accent.withAlpha (0.25f));
    g.strokePath (arc, juce::PathStrokeType (stroke + 4, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (arc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float cr = r * 0.62f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a4330), c.x - cr * 0.4f, c.y - cr * 0.6f, torref, c.x + cr, c.y + cr, true));
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
    g.setColour (line);
    g.drawEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2, 1.0f);
    juce::Path needle;
    needle.addRoundedRectangle (-1.3f, -cr + 2, 2.6f, cr * 0.55f, 1.3f);
    g.setColour (creme);
    g.fillPath (needle, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void epices::LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour (on ? safran : (down ? cacao : (over ? cacao.withAlpha (0.8f) : cannelle)));
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (on ? safran : line);
    g.drawRoundedRectangle (r, 9.0f, 1.0f);
}

void epices::LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);
    auto r = b.getLocalBounds().toFloat();
    auto sw = juce::Rectangle<float> (0, 0, 38, 22).withCentre ({ r.getCentreX(), r.getY() + 16 });
    const bool on = b.getToggleState();
    g.setColour (on ? accent.withAlpha (0.25f) : poivre);
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
    g.setColour (poivre);
    g.fillRoundedRectangle (r, 8);
    g.setColour (box.hasKeyboardFocus (false) ? safran : line);
    g.drawRoundedRectangle (r, 8, 1);
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
        g.setColour (safran);
        g.fillRoundedRectangle (r, 10);
    }
    else if (over)
    {
        g.setColour (cannelle);
        g.fillRoundedRectangle (r, 10);
    }
    g.setColour (front ? poivre : sable);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText().toUpperCase(), r, juce::Justification::centred);
}

//==============================================================================
void WaveView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b1b0e), r.getCentreX(), r.getBottom(), juce::Colour (0xff120d09), r.getCentreX(), r.getY(), true));
    g.fillRoundedRectangle (r, 10);
    g.setColour (epices::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 10, 1);

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
        g.strokePath (p, juce::PathStrokeType (thick));
    };
    for (int f = 7; f >= 0; f--) drawFrame (f / 7.0f, f * 5.0f, -f * 4.0f, epices::line.withAlpha (0.7f), 1.0f);
    const auto col = isA ? epices::safran : epices::paprika;
    drawFrame (pos, pos * 35, -pos * 28, col.withAlpha (0.3f), 6.0f);
    drawFrame (pos, pos * 35, -pos * 28, col, 2.2f);
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
    g.setGradientFill (juce::ColourGradient (epices::cacao.withAlpha (0.55f), 0, 0, epices::torref.withAlpha (0.85f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 14);
    g.setColour (epices::line);
    g.drawRoundedRectangle (r, 14, 1);
    g.setColour (accent);
    g.fillRoundedRectangle (kPad, 13, 4, 12, 2);
    g.setColour (epices::creme);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (title.toUpperCase(), kPad + 10, 8, getWidth() - 30, 22, juce::Justification::centredLeft);
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

    const juce::Colour mc[4] = { epices::safran, epices::paprika, epices::curcuma, epices::cardamome };
    for (int m = 0; m < 4; m++)
    {
        macro[m].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        macro[m].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        macro[m].setColour (juce::Slider::trackColourId, mc[m]);
        macroAtt[m] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "macro" + juce::String (m + 1), macro[m]);
        addAndMakeVisible (macro[m]);
        macroLabel[m].setJustificationType (juce::Justification::centred);
        macroLabel[m].setFont (juce::FontOptions (13.0f, juce::Font::bold));
        macroLabel[m].setColour (juce::Label::textColourId, epices::creme);
        addAndMakeVisible (macroLabel[m]);
    }

    using G = ParamGroup;
    auto osc = std::make_unique<ParamPage>();
    for (bool a : { true, false })
    {
        const juce::String o = a ? "oscA." : "oscB.";
        auto g = std::make_unique<G> (proc, a ? "Osc A" : "Osc B", a ? epices::safran : epices::paprika,
                                      juce::StringArray { o + "on", o + "table", o + "warpMode", o + "pos", o + "level", o + "semi", o + "fine", o + "unison", o + "detune", o + "spread", o + "warp" },
                                      juce::StringArray { "On", "Table", "Warp", "Position", "Niveau", "Demi-ton", "Fin", "Unisson", "Detune", "Stereo", "Warp" });
        auto wv = std::make_unique<WaveView> (proc, a);
        (a ? waveA : waveB) = wv.get();
        g->addTop (std::move (wv), 110);
        osc->add (std::move (g));
    }
    osc->add (std::make_unique<G> (proc, "Sub / Bruit / FM", epices::curcuma,
                                   juce::StringArray { "sub.on", "sub.oct", "sub.shape", "sub.level", "noise.level", "fm.amount" },
                                   juce::StringArray { "Sub", "Octave", "Forme", "Sub", "Bruit", "FM B>A" }));

    auto filt = std::make_unique<ParamPage>();
    filt->add (std::make_unique<G> (proc, "Filtre", epices::paprika,
                                    juce::StringArray { "filter.on", "filter.type", "filter.cutoff", "filter.res", "filter.drive", "filter.env", "filter.keytrack" },
                                    juce::StringArray { "On", "Type", "Cutoff", "Reso", "Drive", "Env 2", "Key" }));
    filt->add (std::make_unique<G> (proc, "Env 1 - Amp", epices::cardamome, juce::StringArray { "env1.a", "env1.d", "env1.s", "env1.r" }, juce::StringArray { "Attaque", "Decay", "Sustain", "Release" }));
    filt->add (std::make_unique<G> (proc, "Env 2 - Mod", epices::badiane, juce::StringArray { "env2.a", "env2.d", "env2.s", "env2.r" }, juce::StringArray { "Attaque", "Decay", "Sustain", "Release" }));

    auto modp = std::make_unique<ParamPage>();
    for (int l = 1; l <= 2; l++)
    {
        const juce::String pre = "lfo" + juce::String (l) + ".";
        modp->add (std::make_unique<G> (proc, "LFO " + juce::String (l), l == 1 ? epices::curcuma : epices::badiane,
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
        modp->add (std::make_unique<G> (proc, half ? "Matrice 5-8" : "Matrice 1-4", epices::safran, ids, labels));
        juce::StringArray amts, amtL;
        for (int s = 1 + half * 4; s <= 4 + half * 4; s++) { amts.add ("mod" + juce::String (s) + ".amt"); amtL.add ("Qte " + juce::String (s)); }
        modp->add (std::make_unique<G> (proc, half ? "Quantites 5-8" : "Quantites 1-4", epices::safran, amts, amtL));
    }

    auto fx = std::make_unique<ParamPage>();
    fx->add (std::make_unique<G> (proc, "Distorsion", epices::piment, juce::StringArray { "fx.dist.on", "fx.dist.type", "fx.dist.drive", "fx.dist.mix" }, juce::StringArray { "On", "Type", "Drive", "Mix" }));
    fx->add (std::make_unique<G> (proc, "Squash", epices::safran, juce::StringArray { "fx.comp.on", "fx.comp.amount", "fx.comp.mix" }, juce::StringArray { "On", "Squash", "Mix" }));
    fx->add (std::make_unique<G> (proc, "Delay", epices::cardamome, juce::StringArray { "fx.delay.on", "fx.delay.div", "fx.delay.pingpong", "fx.delay.fb", "fx.delay.mix" }, juce::StringArray { "On", "Temps", "Ping-pong", "Feedback", "Mix" }));
    fx->add (std::make_unique<G> (proc, "Reverb", epices::badiane, juce::StringArray { "fx.reverb.on", "fx.reverb.size", "fx.reverb.damp", "fx.reverb.mix" }, juce::StringArray { "On", "Taille", "Amorti", "Mix" }));

    auto play = std::make_unique<ParamPage>();
    play->add (std::make_unique<G> (proc, "Jeu", epices::safran, juce::StringArray { "global.legato", "global.poly", "global.glide", "global.bend", "global.volume" }, juce::StringArray { "Legato", "Voix", "Glide", "Bend", "Volume" }));

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
    g.fillAll (epices::poivre);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a2210), getWidth() * 0.5f, -60.0f, epices::poivre, getWidth() * 0.5f, 300.0f, true));
    g.fillRect (getLocalBounds());
    g.setColour (epices::safran);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText (juce::CharPointer_UTF8 ("\xc3\xa9pices"), 22, 12, 130, 40, juce::Justification::centredLeft);
    g.setColour (epices::sable);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("M  A  S  A  L  A", 150, 14, 150, 40, juce::Justification::centredLeft);

    // colonne des macros
    auto mr = juce::Rectangle<float> (16, 74, 150, (float) getHeight() - 90);
    g.setColour (epices::cannelle.withAlpha (0.7f));
    g.fillRoundedRectangle (mr, 14);
    g.setColour (epices::line);
    g.drawRoundedRectangle (mr, 14, 1);
    g.setColour (epices::muted);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("MACROS", mr.removeFromTop (30), juce::Justification::centred);
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
    menu.addSubMenu (juce::CharPointer_UTF8 ("Compte \xc3\xa9pices (cloud)"), cloud, cloud.getNumItems() > 0);
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
