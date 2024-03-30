//
// general
//

export type SourceLocation = {
	path: string,
	line: number,
	startColumn: number,
	endColumn: number,
}

//
// tokens
//

export enum TokenType {
	number,
	string,
	word,
	
	separator,
	operator,
	
	builtinIndicator,
	selfReference,
}

export type Token = {
	type: TokenType,
	text: string,
	location: SourceLocation,
}

//
// AST
//

type genericASTnode = {
	location: SourceLocation,
}

export type ASTnode = 

//
// literals
//

genericASTnode & {
	type: "bool",
	value: boolean,
} | genericASTnode & {
	type: "number",
	value: number,
} | genericASTnode & {
	type: "string",
	value: string,
} |

//
// things you get a value from
//

genericASTnode & {
	type: "identifier",
	name: string,
} | genericASTnode & {
	type: "call",
} |

//
// structured things (what are these called?)
//

genericASTnode & {
	type: "definition",
	mutable: boolean,
	name: string,
	value: ASTnode[],
} | genericASTnode & {
	type: "assignment",
	left: ASTnode[],
	right: ASTnode[],
} | genericASTnode & {
	type: "function",
} | genericASTnode & {
	type: "struct",
} | genericASTnode & {
	type: "while",
} | genericASTnode & {
	type: "if",
} | genericASTnode & {
	type: "return",
}

//
// scope
//

type genericScopeObject = {
	originLocation: SourceLocation,
}

export type ScopeObject = genericScopeObject & {
	type: "bool",
	value: boolean,
} | genericScopeObject & {
	type: "number",
	value: number,
} | genericScopeObject & {
	type: "string",
	value: string,
} | genericScopeObject & {
	type: "complexValue",
	// TODO
} | genericScopeObject & {
	type: "alias",
	mutable: boolean,
	name: string,
	value: ScopeObject[],
} | genericScopeObject & {
	type: "function",
	// TODO
} | genericScopeObject & {
	type: "struct",
	// TODO
}