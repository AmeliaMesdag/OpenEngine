<p align="center">
  <img src="Apps/Editor/Assets/Icons/logo.svg" alt="Open Engine logo" width="160" height="160">
</p>

<h1 align="center">Open Engine</h1>

A modern, high-performance, open-source game engine built for the community, by the community, with the goals of bringing power, scale, ease of use and true ownership to game developers.

It also builds for the web: the same engine runs in the browser on WebGPU, shipped as the `@game-crafters-guild/openengine-web` package with live examples. Its Web API is designed to bring the capabilities and performance of the engine into an HTML canvas anywhere.

**Platforms:** Windows x64 and ARM64 (primary), macOS on Apple Silicon and Intel (native Metal renderer), Linux, Steam Deck.

![The Open Engine editor on a stylized village scene at evening: long shadows across the houses, a lake below, and the post-processing inspector open](docs/images/editor/editor-overview.webp)

## Why Open Engine

Game developers need a powerful, modern engine that is theirs: open source, owned by the community that uses it, and built on current ideas about how an engine should work. The engines most games are built on belong to a few companies, and every developer has felt what that means. Terms change underneath a game after it ships, or in the weeks before, and a team that did nothing wrong is left re-planning a launch it does not control. A feature you depend on is cancelled because someone decided it would not bring in enough money. The change you need waits for years because it never would. It is your game, and you had no say.

Open Engine cannot do any of that, by construction. It is released under the MIT licence with no product, no store and no monetization inside the engine, so there is nothing to protect and nothing to chase. The terms cannot change underneath anyone because there are no terms beyond the licence. A change is made because it is right for the engine and the games built on it, or it is not made; money never enters the decision. The people building it today are the people who want to use it.

Open Engine is built on today's ideas about how an engine should work, with the emphasis on performance, visuals and the experience of using it: a data-oriented core that keeps large worlds fast, a renderer designed around the current generation of GPUs, scripting that recompiles and swaps into the running editor without a restart or a hitch, and an editor whose entire interface is styles and markup rather than code, readable, editable and reloaded live as you change them. The open-source engines that existed were designed around object trees and node hierarchies, and they strain at scale unless large parts are rewritten. The people behind Open Engine have built engines before, inside and outside the large companies; this is the one they wanted to use and could not find.

It is early, and every page says so. But a community of people who want the same thing is already forming around it, and the aim is simple: make building games easier, not harder.

### Why AI is part of it

The community is divided about AI, polarized even, and often for good reasons. The creator of Open Engine has had a difficult relationship with it too. But it is here to stay, and modern LLMs have become one of the most useful collaborators a developer can have, most of all for a developer working alone. With the right tools, one person can now do the work of a small team. The quality of the result still comes from the developer, not the AI: the AI amplifies, and the difference between a quality game and slop is the passion and attention to detail the developer puts in. What the engine can do is give the assistant real access, so that effort reaches the result instead of being lost in the handoff.

The established engines have not caught up. Open Engine is built so that an assistant can reach everything a person can. It has had a complete command-line and MCP interface since before the large engines offered anything comparable: an assistant can open scenes, query entities, move the camera, take screenshots, read GPU and CPU resources and timings, emulate input, inspect the interface, and build whole areas of a scene from mark-ups drawn in the world.

And you bring your own. There is no engine-branded token to buy and no middleman forwarding your prompts to a weaker model. Pair the assistant you already have with the editor, inside the window or from your own terminal; it sees the same scene, the same mark-ups and the same resources either way.

## Foundation

- Open source, MIT licensed.
- A modern C++20 code base.
- Hot-reloadable C# scripting on CoreCLR (.NET 10).
- Native C++ scripting.
- A modern renderer built on Vulkan, Metal and WebGPU.
- A cross-platform editor for Windows, macOS, Linux and the web. Yes, the editor runs in the browser.
- An ECS built on a true data-oriented design.
- UI driven by CSS layout and XML, in the editor and in games.
- Built for AI-assisted development: a bundled MCP server and CLI let AI agents drive the running editor, and an in-editor AI assistant ships as a preview. See [docs/getting-started/mcp-setup.mdx](docs/getting-started/mcp-setup.mdx).

## Try it in the browser

Live examples, built from the engine and published with each release (WebGPU):

[![The model viewer: a sample model under cascaded shadows and a physical sky](docs/images/web/model-viewer.webp)](https://game-crafters-guild.github.io/opengine-web/package/examples/model-viewer/index.html)

- [Model viewer](https://game-crafters-guild.github.io/opengine-web/package/examples/model-viewer/index.html): sample models or your own `.glb`, orbit and pan, time of day, exposure, and the model's animation clips.
- [Instancing](https://game-crafters-guild.github.io/opengine-web/package/examples/instancing/index.html): one model drawn up to 100,000 times, each copy an entity.

The library behind them is [`@game-crafters-guild/openengine-web`](https://www.npmjs.com/package/@game-crafters-guild/openengine-web) (`npm install @game-crafters-guild/openengine-web@alpha`); its README is the guide. The examples' source and the built package live in [opengine-web](https://github.com/Game-Crafters-Guild/opengine-web).

## Build

Prerequisites on Windows: Visual Studio 2026 with the *Desktop development with C++* workload (includes CMake, Ninja and Git), the .NET 10 SDK and a Vulkan-capable GPU driver.

```bat
git clone https://github.com/Game-Crafters-Guild/OpenEngine.git
cd OpenEngine
cmake --preset vs2026-x64-local
cmake --build --preset vs2026-x64-local --config DebugFast --target Editor -j 12
build\vs2026-x64-local\bin\DebugFast\Apps\Editor\Editor.exe
```

The first configure bootstraps vcpkg and builds the third-party dependencies once (expect tens of minutes); every build after that is incremental. macOS and Linux build the same Editor through their own presets (`macos-arm64-local`, `linux-x64-local`); platform prerequisites, the Ninja fast build, Steam Deck, tests and troubleshooting are in [BUILD.md](BUILD.md). With the editor running, [Your first project](docs/getting-started/first-project.mdx) creates a project, saves a scene and exports a game.

## Repository layout

```
Engine/               The engine library: Include/, Source/, and one library per subsystem under Modules/
  Assets/             Engine content the build stages beside the Editor and the Player: materials, render pipelines, textures, UI, graphs
  ECSModules/         Shared ECS components (Audio, Components, Rendering)
Apps/Editor/          The editor (C++, with CSS/XML UI)
Apps/Player/          The game runtime the editor exports games with
Apps/WebLibrary/      The web build: the WebGPU engine module and the @game-crafters-guild/openengine-web library
Managed/              C# side: CoreBridge, HotReload, Scripting.ABI, CompileServerHost
Packages/             Add-on packages: the AI assistant, version-control integrations, post effects, import tools
Examples/             Standalone demos (BUILD_EXAMPLES)
Tests/                Integration tests (module unit tests live next to each module)
ThirdParty/           Bundled third-party code, each with its own license
cmake/                Build scripts, presets and ports
mcp/                  The MCP server and CLI that drive a running editor
docs/                 Documentation (start at docs/README.md)
Tools/                Developer tools and scripts
dependencies/         Auto-provisioned vcpkg checkout (gitignored)
build/                Build trees, one per preset (gitignored)
```

## Community

This is an alpha: APIs and file formats may change between releases.

The engine is yours as much as ours: open issues, send pull requests, and help shape it. [CONTRIBUTING.md](CONTRIBUTING.md) and [CodingStyle.md](CodingStyle.md) say how.

Join us on Discord: https://discord.gg/c8JASm4q8

## A note on AI

AI tools were used in the development of Open Engine. The engine is not "vibe coded": every change draws on its developers' experience and passes human review before it lands, and the same review stands at the door for contributions. Pull requests are judged on design, correctness and evidence, whoever or whatever wrote them; generated code that is not understood, tested and explained by its author is not accepted.

The team welcomes the responsible use of AI. The engine ships with built-in MCP tools and integrations to enable agentic workflows driven by human creativity and design.

## License

MIT, see [LICENSE](LICENSE). The people behind the engine are named in [AUTHORS.md](AUTHORS.md). Third-party code keeps its own license, stated next to it: the libraries under `Engine/Source/ThirdParty/` and `ThirdParty/`, the benchmark sources under `Tools/Benchmarks/ECS/third_party/`, and the dependencies installed through vcpkg.

## Tests

Tests are Google Test executables, one per module; [BUILD.md](BUILD.md) lists how to build and run them. Two package-manager tests clone a live git repository and are skipped unless `GE_GIT_E2E=1` is set: `PackageGitResolution.LiveEndToEndAgainstGameEnginePackagesRepo` and `PackageManagementLive.AddRepinCheckAgainstGameEnginePackagesRepo`.
