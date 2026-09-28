# Builder prompts

Authority: Simon's latest direction on 27 September 2026. Give builders concise, outcome-focused prompts and freedom to make design assumptions. These prompts supersede the prescriptive builder-facing instructions in UI-FIRST-DELIVERY.md and earlier briefs. Existing acceptance evidence, known defects and full-beta scope remain useful to Astra for integration and review; they are not a mandatory design recipe.

## Claude — UI

Build a brand-new UI for our Unreal Engine remake of Star Trek: Birth of the Federation. Use the original game as inspiration for what the interface needs to include and how players use it, while modernizing the whole experience.

Treat the current prototype UI as disposable. Preserve useful backend connections, but feel free to replace its layout, navigation and interactions completely. Aim for an interface at least as capable as the original, recognizably BOTF, and enjoyable to use as a modern strategy game.

Study the original screens and the current project, make your own design decisions, and implement the result. Give particular care to the galaxy map, fleet orders and system management. Deliver working interactions, not just a visual reskin. If the backend cannot support something important, identify what you need.

## Gemini — campaign

Build the gameplay for a complete, playable modern remake of Star Trek: Birth of the Federation in the existing Unreal project. Preserve the original's strategic depth, identity and campaign loop while correcting defects and improving usability.

Use the original game and available research as your reference. Build on useful existing work, connect systems into a coherent campaign, and make your own implementation decisions. Aim for every major system to be at least as capable as the original; a simplified standalone demonstration is not enough.

Work on the campaign area supplied with this prompt, fix its known blockers, and leave it ready to integrate and play. Verify your work and explain any remaining limitations or uncertain departures from the original.

## How Astra dispatches these

Simon clarified again on 27 September: receive each builder's completed output, review the whole delivered section with independent code review, relevant tests and UI visual checks, then send one consolidated repair prompt covering all findings. Do not repeatedly dispatch one defect at a time. Keep broad implementation freedom; the repair brief supplies evidence and desired outcomes rather than an implementation recipe.

Simon clarified: use the same broad, basic style for Gemini's substantial build tasks. Give it a wide outcome and room to decide how to achieve it. Detailed directions are appropriate for a specific iteration, reproduced defect, failed test or necessary shared interface; do not turn broad assignments back into long implementation recipes.

Use the appropriate prompt above, followed by a short task-specific context block: the current objective, relevant files/reference locations, files currently owned by that builder, and any concrete failing reproduction. Supply exact shared APIs only where needed to connect independently built systems. Do not paste the entire acceptance matrix, accumulated review history or a long implementation recipe into every assignment.

Claude owns UI design and implementation decisions. Gemini owns implementation decisions within its campaign assignment. Astra coordinates exclusive ownership, resolves shared contracts, runs integration and packaging, and arranges independent Sol/Luna review. Existing permissions and project boundaries still apply. Send observed defects back as evidence, not a new design prescription.

Latest Simon authorization: use three concurrent Gemini builders with separate ownership, and Sol/Luna at LOW effort for research and bounded implementation as well as reviews. Conserve Claude. Each piece still requires independent review by someone other than its builder. This supersedes the earlier reviewer-only Sol/Luna division for this phase.
