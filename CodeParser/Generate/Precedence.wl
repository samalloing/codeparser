BeginPackage["CodeParser`Generate`Precedence`"]

Begin["`Private`"]

Needs["CodeParser`Generate`GenerateSources`"]

Print["Generating Precedence..."]

oldPrecedences = Join[Names["Precedence`*"], Names["Precedence`*`*"]]

(*
resolve the symbolic values in the Precedence table to integer values
*)
cur = 0
enumMap = <||>
KeyValueMap[(
  Which[
    #2 === 0, cur = 0,
    #2 === Next, cur++,
    True, cur = enumMap[#2]
  ];
  AssociateTo[enumMap, #1 -> cur])&
  ,
  importedPrecedenceSource
]

(*
sanity check that all precedences are in order
*)
cur = -Infinity;
KeyValueMap[
  If[!TrueQ[#2 >= cur],
    Print["Precedence is out of order: ", #1->#2];
    Quit[1]
    ,
    cur = #2
  ]&
  ,
  enumMap
]


precedenceCPPHeader = {
"
//
// AUTO GENERATED FILE
// DO NOT MODIFY
//

#pragma once

#include <cstdint> // for uint8_t

enum Precedence : uint8_t {"} ~Join~
   KeyValueMap[(Row[{toGlobal[#1], " = ", Floor[#2], ","}]) &, enumMap] ~Join~ {"};", ""}

Print["exporting Precedence.h"]
res = Export[FileNameJoin[{generatedCPPIncludeDir, "Precedence.h"}], Column[precedenceCPPHeader], "String"]

If[FailureQ[res],
  Print[res];
  Quit[1]
]

Print["Done Precedence"]

End[]

EndPackage[]