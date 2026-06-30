# ULRE Policy Guard MCP Server Setup Guide

## Installation

### Prerequisites
- nvm-windows (already installed)
- Node.js 20+ (recommended)

### Step 1: Install and select Node with nvm-windows

```bash
nvm install 20.18.0
nvm use 20.18.0
node -v
npm -v
```

### Step 2: Install MCP server dependencies

```bash
cd e:\ULRE\.ai\mcp\policy-guard-ts
npm install
```

### Step 3: Verify Build

```bash
npm run build
```

---

## Running the Server

### Method A: TypeScript Dev Mode
```bash
cd e:\ULRE\.ai\mcp\policy-guard-ts
npm run dev
```

### Method B: VS Code Task (Automated)
1. Press `Ctrl+Shift+D` in VS Code  
2. Select **"Run ULRE Policy Guard MCP Server"** from the task list  
3. A terminal will open with the server running in the background

### Method C: TypeScript Build + Node
```bash
cd e:\ULRE\.ai\mcp\policy-guard-ts
npm run build
npm start
```

## Integration with AI Tools

### For Claude Desktop (claude_desktop_config.json)
Add to your `~\.claude` or `%APPDATA%\Claude` config:

```json
{
  "mcpServers": {
    "ulre-policy-guard": {
      "command": "node",
      "args": ["E:\\ULRE\\.ai\\mcp\\policy-guard-ts\\dist\\index.js"],
      "type": "stdio"
    }
  }
}
```

### For VS Code Copilot
The MCP server runs via the VS Code Task. When the task is running, agents can invoke its tools.

1. **Start the server**: Press `Ctrl+Shift+D` → Select "Run ULRE Policy Guard MCP Server"
2. **Agent discovery**: Copilot will auto-detect available MCP tools once the server is running

### For Custom Agents
Your agent framework can invoke tools via stdio:

```bash
node .ai/mcp/policy-guard-ts/dist/index.js
```

The server exposes:
- **16 tools**: logger/hash/C-string/ULRE-string workspace scan, file check, code review, and guide retrieval
- **8 resources**: logger/hash/C-string/ULRE-string best-practice guides and forbidden pattern lists
- **4 prompt templates**: logger usage review + hash usage review + C-string usage review + ULRE string usage review

---

## Auto-Start on Project Open

### Option 1: Manual Task Launch
Press `Ctrl+Shift+D` and select the task each time. VS Code remembers the terminal.

### Option 2: Auto-Run Task (Experimental)
Add to `.vscode/settings.json`:
```json
{
  "tasks.runOptions": {
    "runOn": "folderOpen"
  }
}
```
Then configure the ULRE Policy Guard task with `"runOptions": {"runOn": "folderOpen"}` in tasks.json.

**Note**: This requires VS Code 1.75+.

### Option 3: Workspace Launch Configuration
Create a `.vscode/launch.json` entry that pre-starts the MCP server before debugging sessions.

---

## Troubleshooting

**"node is not recognized"**
→ Run `nvm use 20.18.0` and reopen VS Code terminal

**"Cannot find module '@modelcontextprotocol/sdk'"**
→ Run `npm install` in `.ai/mcp/policy-guard-ts`

**Server starts but agents can't find tools**
→ Ensure the terminal shows "Listening on stdio" before invoking agents

**Port conflicts**
→ The server uses stdio (no port). If you see connection errors, restart the terminal and the server

---

## File Locations

- TypeScript server: `.ai/mcp/policy-guard-ts/src/index.ts`
- Built entry: `.ai/mcp/policy-guard-ts/dist/index.js`
- Config: `.vscode/tasks.json` (VS Code integration)  
- Dependencies: `.ai/mcp/policy-guard-ts/package.json`  
- This guide: `.ai/mcp/SETUP.md`

---

## Quick Reference

| Task | Command |
|------|---------|
| Install Node deps | `npm install --prefix .ai/mcp/policy-guard-ts` |
| Run server (dev) | `npm run dev --prefix .ai/mcp/policy-guard-ts` |
| Run server (build) | `npm run build --prefix .ai/mcp/policy-guard-ts && npm start --prefix .ai/mcp/policy-guard-ts` |
| Start from VS Code | `Ctrl+Shift+D` → "Run ULRE Policy Guard MCP Server" |
| Check server status | Look for "Listening on stdio" in terminal |
| List server tools | Connect with any MCP client; server exposes 16 tools |
