# SpecialAgent

**Connect AI to Unreal Engine 5**

Local Python API access • 34 exposed editor tools • Visual feedback loop

---

## What is SpecialAgent?

SpecialAgent bridges AI assistants and Unreal Engine 5 through the **Model Context Protocol (MCP)**. Connect Claude, GPT, or any MCP-compatible LLM directly to your editor and control it through natural language.

At its core, SpecialAgent provides local Python execution with UE5's Python API, guarded by loopback-only HTTP access and optional token authentication. On top of that foundation, 34 currently exposed tools handle common level design tasks without writing a single line of code.

Native streamable HTTP transport. No external bridges or dependencies.

---

## Features

### Two Layers of Power

#### Full Python Access

Execute arbitrary Python with complete `unreal` module access. Your AI assistant can:

- Import and process assets
- Create and modify Blueprints  
- Generate materials and textures
- Automate project configuration
- Build custom editor utilities
- Run validation and QA checks
- Anything the UE5 Python API supports

This is the unlimited foundation. If you can script it, AI can do it.

#### Level Design Toolkit

34 exposed tools for world-building workflows:

| Category | Capabilities |
|----------|-------------|
| **Actors** | Spawn, transform, duplicate, delete, list, tag search |
| **Assets** | Content Browser search, metadata, bounds, properties |
| **Viewport** | Camera transform, actor focus, screen-to-world traces |
| **Screenshots** | Capture and save bounded viewport images |
| **Organization** | Folders, tags, labels, selection management |

### Visual Feedback Loop

Capture viewport screenshots and return them to vision-enabled LLMs. Your AI assistant can see what it built, evaluate the results, and refine its approach.

```
Describe intent → Execute → Screenshot → AI analyzes → Iterate
```

---

## Installation

### Requirements

- Unreal Engine 5.6 or later
- Windows, Mac, or Linux
- MCP-compatible client (Cursor, Claude Desktop, etc.)

### Setup

1. **Clone or download** this repository into your project's `Plugins` folder:
   ```
   YourProject/
   └── Plugins/
       └── SpecialAgent/
   ```

2. **Regenerate project files** (right-click `.uproject` → Generate Visual Studio/Xcode project files)

3. **Build and launch** your project

4. **Enable the plugin** in Edit → Plugins → Search "SpecialAgent"

5. **Restart** the editor

---

## Quick Start

### 1. Verify the Server

Once the editor launches, check the Output Log for:

```
SpecialAgent: MCP HTTP Server started on port 8767
```

Or test with curl:

```bash
curl http://localhost:8767/health
```

### 2. Configure Your MCP Client

Add SpecialAgent to your MCP client configuration:

```json
{
  "mcpServers": {
    "SpecialAgent": {
      "url": "http://localhost:8767/mcp"
    }
  }
}
```

### 3. Connect and Build

Your AI assistant now has access to:
- Python execution with full UE5 API
- 34 exposed editor tools
- Viewport screenshot capture
- Editor utilities (save, undo, redo)

---

## Service Categories

| Service | Methods | Description |
|---------|:-------:|-------------|
| **Python** | 3 | Execute scripts, run files, list modules |
| **Screenshot** | 2 | Capture viewport for AI vision |
| **World** | 11 | Actor manipulation and spatial queries |
| **Assets** | 6 | Content Browser search and inspection |
| **Viewport** | 5 | Camera control, actor focus, screen traces |
| **Utility** | 7 | Save, undo, redo, selection tools |
| **Experimental services** | Direct only | Landscape, foliage, lighting, streaming, performance, navigation, and gameplay wrappers are not currently exposed in `tools/list` |

---

## Example Workflows

### Populate a Forest (via Tools)

```
1. assets/search → Find tree and rock assets
2. world/scatter_in_area → Place 500 trees with randomization
3. foliage/paint_in_area → Add grass and ground cover
4. screenshot/capture → Get visual for AI analysis
5. Iterate based on feedback
```

---

## Configuration

Edit `Config/DefaultSpecialAgent.ini` to customize:

```ini
[/Script/SpecialAgent.SpecialAgentSettings]
; Server port (change if 8767 is in use)
ServerPort=8767

; Auto-start server when editor launches
bAutoStart=true

; Enable verbose logging
bVerboseLogging=false
```

---

## Architecture

```
┌─────────────────────────────────────────┐
│        MCP Client (Claude, etc.)        │
└──────────────┬──────────────────────────┘
               │ HTTP + JSON-RPC 2.0
┌──────────────▼──────────────────────────┐
│       SpecialAgent MCP Server           │
│                                         │
│  ┌─────────────────────────────────┐    │
│  │   Python Service (Primary)      │    │
│  │   Full unreal module access     │    │
│  └─────────────────────────────────┘    │
│                                         │
│  ┌─────────────────────────────────┐    │
│  │   34 Exposed Tools              │    │
│  │   Level design & utilities      │    │
│  └─────────────────────────────────┘    │
│                                         │
│  ┌─────────────────────────────────┐    │
│  │   Game Thread Dispatcher        │    │
│  │   Thread-safe API access        │    │
│  └─────────────────────────────────┘    │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│        Unreal Engine 5 Editor           │
└─────────────────────────────────────────┘
```

---

## Documentation

| Document | Description |
|----------|-------------|
| [STRUCTURE.md](STRUCTURE.md) | Plugin architecture and file layout |

---

## Design Philosophy

The exposed tools exist for convenience and discoverability. Python execution is the real power.

When your AI assistant needs custom logic such as density falloff, terrain-aware positioning, or asset variation based on rules, it writes Python.

Both layers work together: quick tools for common tasks, unlimited scripting for everything else.

---

## Troubleshooting

### Server Won't Start

- Check if port 8767 is in use: `netstat -an | grep 8767`
- Change port in `DefaultSpecialAgent.ini`
- Verify plugin is enabled in Edit → Plugins

### Connection Refused

- Ensure Unreal Editor is running
- Check Output Log for server startup messages
- Verify firewall isn't blocking localhost

### Tools Not Appearing

- Call `tools/list` to verify registration
- Check for errors in Output Log
- Restart the editor

### Client not connecting

- Some IDEs like Cursor may need to be started after your Unreal Engine editor as the connection attempt only occurs on startup.

---

## Technical Details

| Specification | Value |
|--------------|-------|
| Engine Version | UE 5.6+ |
| Platforms | Windows, Mac, Linux |
| Module Type | Editor |
| Transport | HTTP (native) |
| Protocol | JSON-RPC 2.0 / MCP |
| Default Port | 8767 |

### Dependencies

- `PythonScriptPlugin` (included with UE5)
- `EditorScriptingUtilities` (included with UE5)

---

## Contributing

Contributions are welcome! Please read the architecture documentation before submitting PRs.

---

## License

MIT License - See LICENSE file for details.

---

*Give your AI assistant the keys to Unreal Engine.*
