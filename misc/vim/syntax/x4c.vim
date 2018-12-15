" Vim syntax file
" Language:      X4 Script
" Maintainer:    roflcopter4
"
if exists('b:current_syntax')
  finish
endif

let s:cpo_save = &cpo
set cpo&vim

syn region x4cBraces start="{" end="}" transparent extend

" All keywords
"
syn keyword x4cConditional	if elsif else
syn keyword x4cRepeat		while for in
syn keyword x4cOperator		defined eq ne ge gt le lt not and or xor not
syn keyword x4cKeyword		let undef add subtract insert
syn keyword x4cKeyword		chance reversed debug
syn keyword x4cKeyword		break return
syn keyword x4cType		error table

syn keyword x4cConstant		NULL true false
syn match x4cObjects		"\%(^\|\s\|[^.A-Za-z0-9_$]\)\@1<=\%(this\|class\|player\|order\|faction\|orderstate\|attention\|event\|command\)"

syn match   x4cOperator /[@?]\|table\[\]\|\[\]/

syn keyword x4cTodo		TODO TODO TBD FIXME XXX NOTE contained containedin=x4cComment

syn region	x4cCommentL	start="//" skip="\\$" end="$" keepend
syn region	x4cComment	matchgroup=x4cCommentStart start="/\*" end="\*/" fold extend

syn match  x4cVarPlain      "\%([$]\|\$#\)\$*\%(\I\i*\)\=\%(\%(::\|'\)\I\i*\)*\%(::\|\i\@<=\)" nextgroup=x4cVarMember,x4cVarSimpleMember,x4cPostDeref,x4cSpecPostDeref
syn match  x4cVarPlain2     "%\$*\%(\I\i*\)\=\%(\%(::\|'\)\I\i*\)*\%(::\|\i\@<=\)"             nextgroup=x4cVarMember,x4cVarSimpleMember,x4cPostDeref,x4cSpecPostDeref

" NotImplemented:
syn match  x4cNotImplementedF +\<\w\+\%(<\)\@=+
syn region x4cNotImplemented matchgroup=x4cNotImplementedStartEnd start="<" end=">" transparent
syn match  x4cNotImplementedAttribute "\<\i\+\%(=\)\@="

" Methods:
syn match x4cMethod "\<\%(\%(\i\|[}\]]\)\+\.\)\@2<=\i\+"


" Special characters in strings and matches
syn match  x4cSpecialString	"\\\%(\o\{1,3}\|x\%({\x\+}\|\x\{1,2}\)\|c.\|[^cx]\)" contained extend
syn match  x4cSpecialStringU	"\\\\" contained
syn match  x4cSpecialMatch	"\\g\%(\d\+\|{\%(-\=\d\+\|\h\w*\)}\)" contained
syn match  x4cSpecialMatch	"\\k\%(<\h\w*>\|'\h\w*'\)" contained

syn match	x4cFormat		display "%\%(\d\+\$\)\=[-+' #0*]*\%(\d*\|\*\|\*\d\+\$\)\%(\.\(\d*\|\*\|\*\d\+\$\)\)\=\%([hlLjzt]\|ll\|hh\)\=\%([aAbdiuoxXDOUfFeEgGcCsSpn]\|\[\^\=.[^]]*\]\|\d\)" contained containedin=x4cString

" Highlight lines with only whitespace (only in blank delimited here documents) as errors
syn match  x4cNotEmptyLine	"^\s\+$" contained
syn match x4cElseIfError	"else\_s*if" containedin=x4cConditional
syn keyword x4cElseIfError	elseif containedin=x4cConditional

" Variable interpolation
syn cluster x4cInterpDQ	contains=x4cSpecialString,x4cVarPlain,x4cVarNotInMatches,x4cVarSlash,x4cVarBlock

" Numbers
syn match  x4cNumber	"\<\%(0\%(x\x[[:xdigit:]_]*\|b[01][01_]*\|\o[0-7_]*\|\)\|[1-9][[:digit:]_]*\)\>"
syn match  x4cFloat	"\<\d[[:digit:]_]*[eE][\-+]\=\d\+"
syn match  x4cFloat	"\<\d[[:digit:]_]*\.[[:digit:]_]*\%([eE][\-+]\=\d\+\)\="
syn match  x4cFloat    "\.[[:digit:]][[:digit:]_]*\%([eE][\-+]\=\d\+\)\="

syn match  x4cString	"\<\%(v\d\+\%(\.\d\+\)*\|\d\+\%(\.\d\+\)\{2,}\)\>" contains=x4cVStringV
syn match  x4cVStringV	"\<v" contained

syn region x4cString            matchgroup=x4cStringStartEnd start="'" end="'" contains=@x4cInterpDQ keepend extend
syn region x4cString		matchgroup=x4cStringStartEnd start=+"+  end=+"+ contains=@x4cInterpDQ keepend extend

hi def link x4cMultiModifiers		perlMulti
hi def link x4cSpecialString		mutedFunc
hi def link MatchGroupStartEnd		perlSpecialChar
hi def link MatchGroupStartEnd2		perlSpecialChar2

hi def link x4cSpecial			Special
hi def link x4cString			String
hi def link x4cStringStartEnd		String
hi def link x4cCharacter		Character
hi def link x4cNumber			Number
hi def link x4cFloat			Float
hi def link x4cIdentifier		Identifier
hi def link x4cLabel			Label
hi def link x4cStatement		Statement
hi def link x4cConditional		Conditional
hi def link x4cKeyword	        	Keyword
hi def link x4cRepeat			Repeat
hi def link x4cOperator			Operator

hi def link x4cConstant			Constant
hi def link x4cObjects			Constant
hi def link x4cMethod			Function

hi def link x4cCommentL			x4cComment
hi def link x4cCommentStart		x4cComment
hi def link x4cComment			Comment

hi def link x4cTodo			Todo
hi def link x4cList			x4cStatement
hi def link x4cVarPlain			x4cIdentifier
hi def link x4cVarPlain2		x4cIdentifier
hi def link x4cArrow			x4cIdentifier
hi def link x4cFiledescRead		x4cIdentifier
hi def link x4cFiledescStatement	x4cIdentifier
hi def link x4cVarSimpleMember		x4cIdentifier
hi def link x4cVarSimpleMemberName 	x4cString
hi def link x4cVarNotInMatches		x4cIdentifier

hi def link x4cFormat			SpecialChar
hi def link x4cNotImplementedStartEnd	Keyword
hi def link x4cNotImplementedF		PreProc
hi def link x4cNotImplementedAttribute	Type
hi def link x4cType			C_Struct

hi def link x4cParensSQ			x4cString
hi def link x4cBracketsSQ		x4cString
hi def link x4cBracesSQ			x4cString
hi def link x4cAnglesSQ			x4cString
hi def link x4cParensDQ			x4cString
hi def link x4cBracketsDQ		x4cString
hi def link x4cBracesDQ			x4cString
hi def link x4cAnglesDQ			x4cString
hi def link x4cElseIfError		Error

syn sync maxlines=100

let b:current_syntax = 'x4c'
let &cpo = s:cpo_save
unlet s:cpo_save

" vim:ts=8:sts=4:sw=4:expandtab:ft=vim
