import { ASTnode, ScopeObject } from "./types";
import { lex } from "./lexer";
import {
	ParserMode,
	parse,
} from './parser';
import { BuilderContext, build } from "./builder";
import utilities from "./utilities";
import { CompileError } from "./report";

export function compileFile(filePath: string) {
	const text = utilities.readFile(filePath);
	// console.log("text:", text);
	
	const tokens = lex(filePath, text);
	// console.log("tokens:", tokens);
	
	let AST: ASTnode[] = [];
	
	try {
		AST = parse({
			tokens: tokens,
			i: 0,
		}, ParserMode.normal, null);
	} catch (error) {
		if (error instanceof CompileError) {
			console.log("could not parse");
			console.log(error.getText(true));
			process.exitCode = 1;
			return;
		} else {
			throw error;
		}
	}
	
	console.log("AST:", JSON.stringify(AST, undefined, 4));
	
	const builderContext: BuilderContext = {
		scopeLevels: [],
		level: -1,
		codeGenText: {},
		filePath: filePath,
	};
	
	try {
		const scopeList = build(builderContext, AST, null, null);
	} catch (error) {
		if (error instanceof CompileError) {
			console.log("uncaught compiler error");
			console.log(error.getText(true));
			process.exitCode = 1;
			return;
		} else {
			throw error;
		}
	}
	
	// console.log("scopeList:", JSON.stringify(scopeList, undefined, 4));
	// console.log("scopeLevels:", JSON.stringify(builderContext.scopeLevels, undefined, 4));
}