# Game Engine Documentation

Welcome to the Game Engine documentation! This directory contains comprehensive guides for developers, users, and contributors.

**Browse locally:** open [`index.html`](index.html). The GitHub Pages workflow (`.github/workflows/docs-site.yml`) publishes nothing and is retired when openengine.dev switches to these pages.

## Writing a page

The documentation is moving to Markdown, readable here on github.com and published on [openengine.dev/docs](https://openengine.dev/docs/) once the site switches to it. The first page in the new form is [The input system](input/input-system.mdx); the other pages are converted in planned batches, and until a page's batch lands an edit keeps it in its current form.

- A page is an `.mdx` file in its area's lower-case folder (`docs/input/`, `docs/rendering/`, ...) with `title` and `description` in its front matter.
- Links are file links relative to the page, with the extension and the anchor: `[Pointer lock](../input/input-system.mdx#pointer-lock)`.
- Every API item with both forms is one code view with a `C++` tab and a `C#` tab (`:::::tabs`, `::::tab[C++]`, `::::tab[C#]`); notes and warnings are `:::note` and `:::caution` asides.
- Images go in the area's `img/` folder.

The full rules, with every directive and the mapping from the HTML pages' styles, are in [`.claude/rules/documentation.md`](../.claude/rules/documentation.md).

## Quick Start

- **[`BUILD.md`](../BUILD.md)** - Build the engine, editor and tests from a fresh clone
- **[`CLAUDE.md`](../CLAUDE.md)** - Working guidance: build, test, debugging, architecture, code style

## Documentation Index

### Using the editor
- **[Your first project](getting-started/first-project.mdx)** - Create or open a project, save a scene, export a game you can run
- **[Game builds and Steam publishing](GamePublishing.html)** - Prepare, verify and upload releases; Steam Deck builds and deployment
- **[Running the Player headless](getting-started/running-headless.mdx)** - No window, graphics device or audio: dedicated servers, simulations, exit codes
- **[The input system](input/input-system.mdx)** - Keys, the mouse and its delta, pointer lock for mouse look; how the editor and the Player deliver input
- **[Navigation Surface](navigation-surface.html)** - Where agents can walk: bake from the terrain, colliders and meshes, read the status, find a path from C++
- **[Editor Keyboard Shortcuts](editor/keyboard-shortcuts.mdx)** - Gizmo tools, camera navigation, tab management
- **[Smart Folders](SmartFolders.md)** - Saved asset queries in the project browser
- **[HDR Post-Processing](rendering/hdr-post-processing.mdx)** - Tonemapping, bloom and volume settings
- **[Spline Walls](editor/spline-walls.mdx)** - Drawing a wall along a spline: corners, slopes and materials
- **[Shader Warm-Up](rendering/shader-compilation.mdx)** - How the editor compiles a project's material shaders in the background, and what can still compile on first use
- **[AI Assistant](editor/ai-assistant.mdx)** - The editor's AI Assistant panel: providers, the conversation, local Claude Code and Codex sessions, letting the assistant act in the editor, and its settings

### Developer Documentation
- **[Documentation hub (HTML)](index.html)** - Entry point for the HTML pages
- **[`CodingStyle.md`](../CodingStyle.md)** - Naming, headers, comments, C++20 idioms
- **[Scripting](scripting/csharp-ecs-systems-architecture.mdx)** - C# systems and components, hot reload, InitializeOnLoad and logging, native C++ scripting
- **[Scripting ABI](scripting/ScriptingABI.md)** - the C ABI between native code and managed scripts
- **[Compile server](scripting/CompileServer.md)** - the compile server and its protocol; [language settings for script authors](scripting/CompileServer.md#language-settings-for-script-authors)
- **[Unity Import](Assets/unity-import.html)** - Converting the scenes of a Unity package into engine scenes with Tools > Import Unity Package, and where the converter comes from
- **[Model Extras](Assets/model-extras.html)** - Reading the custom data a glTF file attaches to its scenes, nodes, meshes, materials and animations from a script
- **[glTF Clip Schema](animation/gltf-clip-schema.mdx)** - Declaring a clip's loop, speed, root motion bone, translation mode and events in a glTF animation's extras
- **[Humanoid Retargeting](animation/humanoid-retargeting.mdx)** - Playing a clip authored on one humanoid skeleton on another: assets, import, auto-bootstrap, what runs each frame
- **[Animation Events](animation/animation-events.mdx)** - When a clip's events fire during playback, and reading them from a script with `AnimatorApi.PollEvents` or from engine code
- **[ECS Parallel Queries](ECS_ParallelQueries.md)** - running a query on the job system, and the rules that come with it
- **[Editor UI Component Library](editor/ui-component-library.mdx)** - Current editor UI controls, classes, states, sizing, and usage patterns
- **[Editor UI Guidelines](contributing/editor-ui-guidelines.mdx)** - Design and implementation rules for authoring consistent editor UI

### AI Integration
- **[MCP Setup](getting-started/mcp-setup.mdx)** - Model Context Protocol setup and configuration: an AI client such as Claude Code, Cursor or Claude Desktop driving the editor; the editor's own [AI Assistant](editor/ai-assistant.mdx) panel attaches the same server

## Architecture Overview

The Game Engine consists of several key components:

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   AI Agents     │◄──►│  TypeScript MCP  │◄──►│  C++ Editor     │
│   (Augment)     │    │     Server       │    │   Application   │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                                │                        │
                                │                        │
                                ▼                        ▼
                       ┌──────────────────┐    ┌─────────────────┐
                       │   Game Scripts   │    │  Core Engine    │
                       │      (C#)        │    │     (C++)       │
                       └──────────────────┘    └─────────────────┘
```

### Key Features

- **C++ Core Engine**: High-performance game engine core
- **C# Scripting**: Hot-reloadable game scripts with CoreCLR integration
- **Visual Editor**: WYSIWYG editor with AI communication overlay
- **AI Integration**: Full AI agent support via Model Context Protocol
- **Visual AI Communication**: AI can display messages and images directly in the editor

## Getting Help

### Common Issues
- See the Troubleshooting section of [`BUILD.md`](../BUILD.md) for build issues
- See [MCP Setup](getting-started/mcp-setup.mdx) for AI integration problems

### Development Support
- Read [`CodingStyle.md`](../CodingStyle.md) for coding standards
- Read [Hot reload and scripting best practices](scripting/hot-reload-best-practices.mdx) for scripting and hot-reload issues
- Review architecture documents for system design

### Contributing
- Follow the rules in [`CONTRIBUTING.md`](../CONTRIBUTING.md)
- Run the test suites for the code you changed (see Tests in [`BUILD.md`](../BUILD.md))
- Update documentation when adding new features

## What exists

Every subsystem below ships and is exercised by the editor. `Engine/Modules/` is the
full list; this is the shape of it.

- **World** — an archetype ECS with SoA chunk storage, change tracking and parallel
  queries; a scene system; a BVH; Jolt physics behind `Physics`/`PhysicsECS`.
- **Rendering** — a Vulkan-first GPU-driven renderer: render graph, bindless
  descriptors, indirect draws, Forward+ clustered lighting, cascaded shadows,
  HDR output and a post-process stack. Metal, DirectX 12 and WebGPU backends exist
  in-tree at varying maturity; Vulkan is the one the editor runs on. On machines with
  an integrated and a discrete GPU the runtime asks for the discrete one (in the
  browser through the high-performance adapter request, on Windows through driver
  hints the Editor and Player executables export); with WebGPU, the log line
  `WebGpuDevice: initialized on ...` names the chosen GPU, with its type and vendor
  where the platform reports them (browsers usually do not).
- **Terrain and nature** — concurrent-binary-tree terrain with its own ECS layer,
  grass, splines, ocean, volumetric clouds and fog.
- **Content** — a GUID-keyed asset database with derived caches, async streaming,
  a dependency graph and targeted hot reload; glTF/FBX/.blend import; animation with
  humanoid retargeting; text shaping; audio; input.
- **Scripting** — CoreCLR hosting with a resident compile server: edit a C# file and
  the running engine swaps the assembly.
- **Editor** — a CSS/XML-driven editor with docking, multi-window, play mode, an
  undo stack, custom inspectors and a debug server the tooling drives.

## Historical / Archived Docs

Most older status-snapshot or "completion" documents have been removed in favor of area-specific overview and design docs (for ECS, Rendering, Scripting/HotReload, etc.). Any remaining historical documents are clearly marked in their own headings and are kept only for context; when in doubt, prefer the current overview/design docs over historical progress reports.

## License

[License information here]
