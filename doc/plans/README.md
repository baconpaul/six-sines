# Plans

Retained output from longer planning sessions.

Some features are worth thinking through properly before any code gets written —
the ones that touch the streaming format, the DSP inner loop, or the threading
model, where the cost of finding out halfway through is high. Those sessions
produce a design document, and this is where it lives.

A plan here records what was decided and, more usefully, *why the alternatives
were rejected*. That second part is the bit that evaporates from a git log. When
the question comes back around in a year — why is that read templated instead of
branched, why is the wavetable payload base64 — the answer is here rather than
lost.

These are historical documents. They are not updated to track the code, and
where they disagree with the source, the source wins. A plan that turned out to
be wrong is still worth keeping; the reasoning is the point.

| Plan | Subject |
|---|---|
| [user-wavetables.html](user-wavetables.html) | Single- and multi-frame user wavetables: FFT-built mip chains, morph, patch embedding |
