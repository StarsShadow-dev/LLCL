import { stdout } from "process";

import {
	SourceLocation,
	ASTnode,
	ScopeObject,
} from "./types";
import utilities from "./utilities";
import { Indicator, displayIndicator, CompileError } from "./report";
import { builtinScopeLevel, builtinCall } from "./builtin";

let nextSymbolName = 0;

export type BuilderContext = {
	scopeLevels: ScopeObject[][],
	level: number,
	codeGenText: any,
	filePath: string,
}

export type BuilderOptions = {
	getAlias: boolean,
	resultAtRet: boolean,
}

function expectType(context: BuilderContext, expected: ScopeObject, actual: ScopeObject, compileError: CompileError) {
	let expectedType: ScopeObject = expected;
	
	let actualType: ScopeObject = {} as ScopeObject;
	if (actual.kind == "bool") {
		const scopeObject = getAlias(context, "Bool");
		if (scopeObject && scopeObject.kind == "alias" && scopeObject.value) {
			actualType = scopeObject.value[0];	
		}
	} else if (actual.kind == "number") {
		const scopeObject = getAlias(context, "Number");
		if (scopeObject && scopeObject.kind == "alias" && scopeObject.value) {
			actualType = scopeObject.value[0];	
		}
	} else if (actual.kind == "string") {
		const scopeObject = getAlias(context, "String");
		if (scopeObject && scopeObject.kind == "alias" && scopeObject.value) {
			actualType = scopeObject.value[0];	
		}
	} else {
		actualType = actual;
	}
	
	if (expectedType.kind == "type" && actualType.kind == "type") {
		if (expectedType.name != actualType.name) {
			compileError.msg = compileError.msg
				.replace("$expectedTypeName", expectedType.name)
				.replace("$actualTypeName", actualType.name);
			throw compileError;
		}
	} else {
		utilities.unreachable();
	}
}

function getAlias(context: BuilderContext, name: string): ScopeObject | null {
	{
		for (let i = 0; i < builtinScopeLevel.length; i++) {
			const scopeObject = builtinScopeLevel[i];
			if (scopeObject.kind == "alias") {
				if (scopeObject.name == name) {
					return scopeObject;
				}
			} else {
				utilities.unreachable();
			}
		}
	}
	for (let level = context.level; level >= 0; level--) {
		for (let i = 0; i < context.scopeLevels[level].length; i++) {
			const scopeObject = context.scopeLevels[level][i];
			if (scopeObject.kind == "alias") {
				if (scopeObject.name == name) {
					return scopeObject;
				}
			} else {
				utilities.unreachable();
			}
		}
	}
	
	return null;
}

function addAlias(context: BuilderContext, level: number, alias: ScopeObject) {
	if (alias.kind == "alias") {
		const oldAlias = getAlias(context, alias.name)
		if (oldAlias) {
			throw new CompileError(`alias '${alias.name}' already exists`)
				.indicator(oldAlias.originLocation, "alias originally defined here")
				.indicator(alias.originLocation, "alias redefined here");
		}
		context.scopeLevels[level].push(alias);
	} else {
		utilities.unreachable();
	}
}

export function build(context: BuilderContext, AST: ASTnode[], options: BuilderOptions | null, sackMarker: Indicator | null): ScopeObject[] {
	context.level++;
	
	let scopeList: ScopeObject[] = [];
	
	if (context.scopeLevels[context.level] == undefined) {
		context.scopeLevels[context.level] = [];	
	}
	
	try {
		if (options) {
			scopeList = _build(context, AST, options);	
		} else {
			scopeList = _build(context, AST, {
				getAlias: false,
				resultAtRet: false,
			});
		}
	} catch (error) {
		if (error instanceof CompileError && sackMarker != null) {
			stdout.write("stack trace ");
			displayIndicator(sackMarker);
		}
		context.scopeLevels[context.level] = [];
		context.level--;
		throw error;
	}
	
	context.scopeLevels[context.level] = [];
	context.level--;
	
	return scopeList;
}

export function _build(context: BuilderContext, AST: ASTnode[], options: BuilderOptions): ScopeObject[] {
	let scopeList: ScopeObject[] = [];
	
	function addToScopeList(scopeObject: ScopeObject) {
		if (!options.resultAtRet) {
			scopeList.push(scopeObject);	
		}
	}
	
	for (let i = 0; i < AST.length; i++) {
		const node = AST[i];
		
		if (node.kind == "definition") {
			addAlias(context, context.level, {
				kind: "alias",
				originLocation: node.location,
				mutable: node.mutable,
				name: node.name,
				value: null,
			});
		}
	}
	
	for (let i = 0; i < AST.length; i++) {
		const node = AST[i];
		
		if (node.kind == "definition") {
			const alias = getAlias(context, node.name);
			if (alias && alias.kind == "alias") {
				const value = build(context, node.value, null, null);
				
				if (node.value[0].kind == "function" && value[0].kind == "function" && value[0].originLocation != "core") {
					value[0].name += `:${value[0].originLocation.path}:${alias.name}`;
				}
				
				alias.value = value;
			} else {
				utilities.unreachable();
			}
			continue;
		}
		
		switch (node.kind) {
			case "bool": {
				break;
			}
			case "number": {
				addToScopeList({
					kind: "number",
					originLocation: node.location,
					value: node.value,
				});
				break;
			}
			case "string": {
				addToScopeList({
					kind: "string",
					originLocation: node.location,
					value: node.value,
				});
				break;
			}
			
			case "identifier": {
				const alias = getAlias(context, node.name);
				if (alias && alias.kind == "alias") {
					if (alias.value) {
						if (options.getAlias) {
							addToScopeList(alias);	
						} else {
							addToScopeList(alias.value[0]);
						}
					} else {
						throw new CompileError(`alias '${node.name}' used before its definition`)
							.indicator(node.location, "identifier here")
							.indicator(alias.originLocation, "alias defined here");
					}
				} else {
					throw new CompileError(`alias '${node.name}' does not exist`).indicator(node.location, "here");
				}
				break;
			}
			case "call": {
				const functionToCall = build(context, node.left, null, null)[0];
				const callArguments = build(context, node.callArguments, null, null);
				
				if (functionToCall.kind == "function") {
					if (callArguments.length > functionToCall.functionArguments.length) {
						throw new CompileError(`too many arguments passed to function '${functionToCall.name}'`)
							.indicator(node.location, "function call here");
					}
					
					if (callArguments.length < functionToCall.functionArguments.length) {
						throw new CompileError(`not enough arguments passed to function '${functionToCall.name}'`)
							.indicator(node.location, "function call here");
					}
					
					for (let index = 0; index < functionToCall.functionArguments.length; index++) {
						const argument = functionToCall.functionArguments[index];
						
						if (argument.kind == "argument") {
							expectType(context, argument.type[0], callArguments[index],
								new CompileError(`expected type $expectedTypeName but got type $actualTypeName`)
									.indicator(callArguments[index].originLocation, "argument here")
									.indicator(argument.originLocation, "argument defined here")
							);
							
							addAlias(context, context.level + 1, {
								kind: "alias",
								originLocation: node.location,
								mutable: false,
								name: argument.name,
								value: [callArguments[index]],
							});	
						}
					}
					
					const result = build(context, functionToCall.AST, {
						getAlias: false,
						resultAtRet: true,
					}, {
						location: functionToCall.originLocation,
						msg: `function ${functionToCall.name}`,
					})[0];
					if (result && functionToCall.returnType) {
						expectType(context, functionToCall.returnType[0], result,
							new CompileError(`expected type $expectedTypeName but got type $actualTypeName`)
								.indicator(node.location, "call here")
								.indicator(functionToCall.originLocation, "function defined here")
						);
					}
					addToScopeList(result);
				} else {
					utilities.unreachable();
				}
				
				break;
			}
			case "builtinCall": {
				const callArguments = build(context, node.callArguments, null, null);
				builtinCall(context, node, callArguments);
				break;
			}
			
			case "assignment": {
				break;
			}
			case "function": {
				let returnType = null;
				if (node.returnType) {
					returnType = build(context, node.returnType, null, null);
				}
				
				let functionArguments: ScopeObject[] = [];
				
				for (let index = 0; index < node.functionArguments.length; index++) {
					const argument = node.functionArguments[index];
					
					if (argument.kind == "argument") {
						const argumentType = build(context, argument.type, null, null);
						
						functionArguments.push({
							kind: "argument",
							originLocation: argument.location,
							name: argument.name,
							type: argumentType,
						});	
					} else {
						utilities.unreachable();
					}
				}
				
				addToScopeList({
					kind: "function",
					originLocation: node.location,
					name: `${nextSymbolName}`,
					functionArguments: functionArguments,
					returnType: returnType,
					AST: node.codeBlock,
				});
				nextSymbolName++;
				break;
			}
			case "struct": {
				break;
			}
			case "while": {
				break;
			}
			case "if": {
				break;
			}
			case "return": {
				if (!options.resultAtRet) {
					throw new CompileError("unexpected return").indicator(node.location, "here");
				}
				const value = build(context, node.value, null, null)[0];
				scopeList.push(value);
				return scopeList;
			}
		
			default: {
				utilities.unreachable();
				break;
			}
		}
	}
	
	return scopeList;
}