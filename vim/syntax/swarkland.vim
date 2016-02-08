" Vim syntax file
" Language: swarkland

if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

syn case match


" actions/directives
syn keyword swarklandAction move wait attack zap pickup drop quaff read_book throw down
syn match   swarklandCheat     /!health\>/
syn match   swarklandCheat     /!kill\>/
syn match   swarklandCheat     /!polymorph\>/
syn match   swarklandCheat     /!monster\>/
syn match   swarklandCheat     /!wish\>/
syn match   swarklandCheat     /!identify\>/
syn match   swarklandCheat     /!down\>/
syn match   swarklandCheat     /!levelup\>/
syn match   swarklandDirective /@test\>/
syn match   swarklandDirective /@seed\>/
syn match   swarklandDirective /@rng\>/
syn match   swarklandDirective /@mark_events\>/
syn match   swarklandDirective /@expect_event\>/
syn match   swarklandDirective /@expect_no_events\>/
syn match   swarklandDirective /@find_things_at\>/
syn match   swarklandDirective /@expect_thing\>/
syn match   swarklandDirective /@expect_nothing\>/
syn match   swarklandDirective /@expect_carrying\>/
syn match   swarklandDirective /@expect_carrying_nothing\>/

" barewords
syn keyword swarklandBareword individual potion wand book
syn keyword swarklandBareword human ogre lich pink_blob air_elemenetal dog ant bee beetle scorpion snake unseen
syn keyword swarklandBareword player ai
syn keyword swarklandBareword confusion digging striking speed remedy unknown
syn keyword swarklandBareword healing poison ethereal_vision cogniscopy blindness invisibility
syn keyword swarklandBareword magic_bullet mapping force assume_form blinding slowing

" patterns
syn match   swarklandNumber /-\?\<\d\{1,10\}\>/
syn match   swarklandNumber /\<\x\{64\}\>/
syn match   swarklandTag    /\<[^ ]\+:[^ ]\+:\S\+/

" regions
syn region  swarklandString start=/"/ skip=/\\"/ end=/"/ contains=@Spell
syn match   swarklandComment /#.*$/ contains=@Spell
syn match   swarklandError "\t"

" what the categories mean
hi def link swarklandNumber Number
hi def link swarklandComment Comment
hi def link swarklandString String
hi def link swarklandAction Keyword
hi def link swarklandCheat Keyword
hi def link swarklandDirective Special
hi def link swarklandBareword Identifier
hi def link swarklandTag Identifier
hi def link swarklandError Error

let b:current_syntax = "swarkland"
