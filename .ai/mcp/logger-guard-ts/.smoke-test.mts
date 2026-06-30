import path from "node:path";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";

const cwd = process.cwd();
const serverCwd = path.resolve(cwd, ".ai/mcp/logger-guard-ts");
const transport = new StdioClientTransport({
  command: "node",
  args: ["dist/index.js"],
  cwd: serverCwd,
});

const client = new Client({ name: "logger-guard-smoke", version: "0.1.0" });

await client.connect(transport);
const tools = await client.listTools();
const resources = await client.listResources();
const prompts = await client.listPrompts();

console.log("TOOLS:", tools.tools.map((t) => t.name).join(", "));
console.log("RESOURCES:", resources.resources.map((r) => r.uri).join(", "));
console.log("PROMPTS:", prompts.prompts.map((p) => p.name).join(", "));

await client.close();
