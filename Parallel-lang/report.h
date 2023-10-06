#ifndef report_h
#define report_h

#include "types.h"
#include "builder.h"

//
// shared
//

extern CharAccumulator reportMsg;
void addStringToReportMsg(char *string);
void addSubStringToReportMsg(SubString *subString);

extern CharAccumulator reportIndicator;
void addStringToReportIndicator(char *string);
void addSubStringToReportIndicator(SubString *subString);

//
// warnings
//

typedef enum {
	WarningType_format,
	WarningType_unused,
	WarningType_redundant,
	WarningType_unsafe,
	WarningType_deprecated
} WarningType;

void compileWarning(ModuleInformation *MI, SourceLocation location, WarningType warningType);

//
// errors
//

void compileError(ModuleInformation *MI, SourceLocation location) __dead2;

#endif /* report_h */