import * as fs from "fs";
import { stdout } from "process";

import {
	ASTnode,
	ScopeObject,
} from "./types";
import { CompileError } from "./report";
import utilities from "./utilities";
import { BuilderContext } from "./builder";
import path from "path";

export let builtinScopeLevel: ScopeObject[] = [];

function addType(name: string) {
	builtinScopeLevel.push({
		kind: "alias",
		mutable: false,
		originLocation: "core",
		name: name,
		value: [
			{
				kind: "type",
				originLocation: "core",
				name: "core:" + name
			}
		]
	});
}

export function setUpBuiltin() {
	addType("Bool");
	addType("Number");
	addType("String");
}

// builtin function constructor
class FC {
	private builtinNode: ASTnode
	private scopeObjects: ScopeObject[]
	private nodes: ASTnode[]
	private i: number
	
	constructor(builtinNode: ASTnode, scopeObjects: ScopeObject[], nodes: ASTnode[]) {
		this.builtinNode = builtinNode;
		this.scopeObjects = scopeObjects;
		this.nodes = nodes;
		this.i = 0;
	}
	
	public "string" = (): string => {
		const scopeObject = this.scopeObjects[this.i];
		const node = this.nodes[this.i];
		
		if (!scopeObject || !node) {
			throw new CompileError("builtin argument error")
				.indicator(this.builtinNode.location, "expected a string but there are no more arguments");
		}
		
		this.i++;
		
		if (scopeObject.kind == "string") {
			return scopeObject.value;
		} else {
			throw new CompileError("builtin argument error")
				.indicator(node.location, "expected a string");
		}
	}
	
	public done() {
		const node = this.nodes[this.i];
		
		if (node) {
			throw new CompileError("builtin argument error")
				.indicator(node.location, "expected no more arguments");
		}
	}
	
	public next(): ScopeObject {
		const scopeObject = this.scopeObjects[this.i];
		return scopeObject;
	}
}

export function builtinCall(context: BuilderContext, node: ASTnode, callArguments: ScopeObject[]): ScopeObject | undefined {
	if (node.kind == "builtinCall") {
		const fc = new FC(node, callArguments, node.callArguments);
		
		if (node.name == "compileLog") {
			let str = "[compileLog] ";
			while (fc.next()) {
				str += fc.string();
			}
			str += "\n"
			fc.done();
			
			stdout.write(str);
		} else if (node.name == "codeGen") {
			let name = fc.string();
			
			let str = "";
			while (fc.next()) {
				str += fc.string();
			}
			fc.done();
			
			if (context.codeGenText[name] == undefined) {
				context.codeGenText[name] = "";
			}
			
			context.codeGenText[name] += str;
		} else if (node.name == "writeTofile") {
			let name = fc.string();
			let outPath = fc.string();
			fc.done();
			
			if (typeof context.codeGenText[name] == "string") {
				const newPath = path.join(path.dirname(context.filePath), outPath);
				const dir = path.dirname(newPath);
				
				if (!fs.existsSync(dir)) {
					fs.mkdirSync(dir, { recursive: true });
				}
				
				fs.writeFileSync(newPath, context.codeGenText[name]);
				context.codeGenText[name] = "";
			} else {
				throw new CompileError(`unable to write '${name}' to file '${outPath}' because '${name}' does not exist`).indicator(node.location, "here");
			}
		}
	} else {
		utilities.unreachable();
	}
	
	return;
}