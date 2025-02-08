/*
 * Six Sines
 *
 * A synth with audio rate modulation.
 *
 * Copyright 2024-2025, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work will be
 * released under GPL3.
 *
 * The source code and license are at https://github.com/baconpaul/six-sines
 */

#ifndef BACONPAUL_SIX_SINES_UI_CLIPBOARD_H
#define BACONPAUL_SIX_SINES_UI_CLIPBOARD_H

#include <vector>
#include "six-sines-editor.h"
#include "synth/patch.h"

namespace baconpaul::six_sines::ui
{
struct Clipboard
{
    Clipboard();

    enum ClipboardType
    {
        NONE,
        ENVELOPE,
        LFO,
        MODULATION,

        SOURCE_FULLNODE,
        MIXER_FULLNODE,
        MATRIX_FULLNODE,
        FEEDBACK_FULLNODE
    } clipboardType{NONE};

    void doCopyFrom(const std::vector<Param *> &pars, ClipboardType type)
    {
        clipboard.clear();
        clipboardType = type;
        for (auto p : pars)
        {
            clipboard.push_back(p->value);
        }
    }

    void doPasteTo(SixSinesEditor &e, const std::vector<Param *> &pars, ClipboardType type) const
    {
        if (clipboardType != type || clipboard.empty())
            return;
        if (pars.size() != clipboard.size())
        {
            SXSNLOG("ERROR - pars and clipboard differ in size");
            return;
        }
        int idx{0};
        for (auto p : pars)
        {
            e.setAndSendParamValue(*p, clipboard[idx++]);
        }
    }

    void doReset(SixSinesEditor &e, const std::vector<Param *> &pars) const
    {
        for (auto p : pars)
        {
            e.setAndSendParamValue(*p, p->meta.defaultVal);
        }
    }

    template <typename Node> void copyEnvelopeFrom(Node &n)
    {
        std::vector<Param *> pars;
        n.appendDAHDSRParams(pars);
        doCopyFrom(pars, ENVELOPE);
    }

    template <typename Node> void pasteEnvelopeTo(SixSinesEditor &e, Node &n) const
    {
        std::vector<Param *> pars;
        n.appendDAHDSRParams(pars);
        doPasteTo(e, pars, ENVELOPE);
    }

    template <typename Node> void resetEnvelopeHelper(SixSinesEditor &e, Node &n) const
    {
        std::vector<Param *> pars;
        n.appendDAHDSRParams(pars);
        doReset(e, pars);
    }

    template <typename Node> void copyLFOFrom(Node &n)
    {
        std::vector<Param *> pars;
        n.appendLFOParams(pars);
        doCopyFrom(pars, LFO);
    }

    template <typename Node> void pasteLFOTo(SixSinesEditor &e, Node &n) const
    {
        std::vector<Param *> pars;
        n.appendLFOParams(pars);
        doPasteTo(e, pars, LFO);
    }

    template <typename Node> void resetLFOHelper(SixSinesEditor &e, Node &n) const
    {
        std::vector<Param *> pars;
        n.appendLFOParams(pars);
        doReset(e, pars);
    }

    template <typename Node> std::vector<Param *> getModulationParams(Node &n) const
    {
        std::vector<Param *> pars;
        n.appendModulationParams(pars);
        for (auto &p : n.modtarget)
        {
            pars.push_back(&p);
        }
        return pars;
    }
    template <typename Node> void copyModulationFrom(Node &n)
    {
        doCopyFrom(getModulationParams(n), MODULATION);
    }

    template <typename Node> void pasteModulationTo(SixSinesEditor &e, Node &n) const
    {
        doPasteTo(e, getModulationParams(n), MODULATION);
    }

    template <typename Node> void resetModulationHelper(SixSinesEditor &e, Node &n) const
    {
        doReset(e, getModulationParams(n));
    }

    template <typename Node> void copyFullNodeFrom(Node &n, ClipboardType type)
    {
        doCopyFrom(n.params(), type);
    }

    template <typename Node>
    void pasteFullNodeTo(SixSinesEditor &e, Node &n, ClipboardType type) const
    {
        doPasteTo(e, n.params(), type);
    }

    template <typename Node> void resetFullNodeHelper(SixSinesEditor &e, Node &n) const
    {
        doReset(e, n.params());
    }

  private:
    std::vector<float> clipboard;
};

struct SupportsClipboard
{
    virtual ~SupportsClipboard() = default;
    virtual void copyEnvelopeTo(Clipboard &c) = 0;
    virtual void pasteEnvelopeFrom(const Clipboard &c) = 0;
    virtual void resetEnvelope(const Clipboard &c) = 0;

    virtual void copyLFOTo(Clipboard &c) = 0;
    virtual void pasteLFOFrom(const Clipboard &c) = 0;
    virtual void resetLFO(const Clipboard &c) = 0;

    virtual void copyModulationTo(Clipboard &c) = 0;
    virtual void pasteModulationFrom(const Clipboard &c) = 0;
    virtual void resetModulation(const Clipboard &c) = 0;

    virtual Clipboard::ClipboardType getFullNodeType() const = 0;

    virtual void copyFullNodeTo(Clipboard &c) = 0;
    virtual void pasteFullNodeFrom(const Clipboard &c) = 0;
    virtual void resetFullNode(const Clipboard &c) = 0;
    virtual bool supportsFullNode() const = 0;
};

// This is a bit gross to do with a macro but the Node variation plus virtuals
// makes it the easiest approach

#define HAS_CLIPBOARD_SUPPORT                                                                      \
    void copyEnvelopeTo(Clipboard &c) override;                                                    \
    void pasteEnvelopeFrom(const Clipboard &c) override;                                           \
    void resetEnvelope(const Clipboard &c) override;                                               \
    void copyLFOTo(Clipboard &c) override;                                                         \
    void pasteLFOFrom(const Clipboard &c) override;                                                \
    void resetLFO(const Clipboard &c) override;                                                    \
    void copyModulationTo(Clipboard &c) override;                                                  \
    void pasteModulationFrom(const Clipboard &c) override;                                         \
    void resetModulation(const Clipboard &c) override;                                             \
    Clipboard::ClipboardType getFullNodeType() const override;                                     \
    void copyFullNodeTo(Clipboard &c) override;                                                    \
    void pasteFullNodeFrom(const Clipboard &c) override;                                           \
    void resetFullNode(const Clipboard &c) override;                                               \
    bool supportsFullNode() const override;

#define IMPLEMENTS_CLIPBOARD_SUPPORT(CN, NODE, CLIPBOARD_TYPE)                                     \
    void CN::copyEnvelopeTo(Clipboard &c) { c.copyEnvelopeFrom(editor.patchCopy.NODE); }           \
    void CN::pasteEnvelopeFrom(const Clipboard &c)                                                 \
    {                                                                                              \
        c.pasteEnvelopeTo(editor, editor.patchCopy.NODE);                                          \
        repaint();                                                                                 \
    }                                                                                              \
    void CN::resetEnvelope(const Clipboard &c)                                                     \
    {                                                                                              \
        c.resetEnvelopeHelper(editor, editor.patchCopy.NODE);                                      \
        repaint();                                                                                 \
    }                                                                                              \
    void CN::copyLFOTo(Clipboard &c) { c.copyLFOFrom(editor.patchCopy.NODE); }                     \
    void CN::pasteLFOFrom(const Clipboard &c)                                                      \
    {                                                                                              \
        c.pasteLFOTo(editor, editor.patchCopy.NODE);                                               \
        repaint();                                                                                 \
    }                                                                                              \
    void CN::resetLFO(const Clipboard &c)                                                          \
    {                                                                                              \
        c.resetLFOHelper(editor, editor.patchCopy.NODE);                                           \
        repaint();                                                                                 \
    }                                                                                              \
    void CN::copyModulationTo(Clipboard &c) { c.copyModulationFrom(editor.patchCopy.NODE); }       \
    void CN::pasteModulationFrom(const Clipboard &c)                                               \
    {                                                                                              \
        c.pasteModulationTo(editor, editor.patchCopy.NODE);                                        \
        repaint();                                                                                 \
    }                                                                                              \
    void CN::resetModulation(const Clipboard &c)                                                   \
    {                                                                                              \
        c.resetModulationHelper(editor, editor.patchCopy.NODE);                                    \
        repaint();                                                                                 \
    }                                                                                              \
    void CN::copyFullNodeTo(Clipboard &c)                                                          \
    {                                                                                              \
        c.copyFullNodeFrom(editor.patchCopy.NODE, CLIPBOARD_TYPE);                                 \
    }                                                                                              \
    void CN::pasteFullNodeFrom(const Clipboard &c)                                                 \
    {                                                                                              \
        c.pasteFullNodeTo(editor, editor.patchCopy.NODE, CLIPBOARD_TYPE);                          \
        repaint();                                                                                 \
    }                                                                                              \
    void CN::resetFullNode(const Clipboard &c)                                                     \
    {                                                                                              \
        c.resetFullNodeHelper(editor, editor.patchCopy.NODE);                                      \
        repaint();                                                                                 \
    }                                                                                              \
    Clipboard::ClipboardType CN::getFullNodeType() const { return CLIPBOARD_TYPE; }                \
    bool CN::supportsFullNode() const { return CLIPBOARD_TYPE != Clipboard::ClipboardType::NONE; }

}; // namespace baconpaul::six_sines::ui

#endif // CLIPBOARD_H
