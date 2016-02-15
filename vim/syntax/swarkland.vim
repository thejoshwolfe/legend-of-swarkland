" Vim syntax file
" Language: swarkland

if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

syn case match


" actions/directives
syn keyword swarklandAction move wait attack zap pickup drop quaff read_book throw down ability
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
syn match   swarklandDirective /@snapshot\>/
syn match   swarklandDirective /@map_tiles\>/
syn match   swarklandDirective /@aesthetics\>/
syn match   swarklandDirective /@rng_state\>/
syn match   swarklandDirective /@thing\>/
syn match   swarklandDirective /@life\>/
syn match   swarklandDirective /@status_effect\>/
syn match   swarklandDirective /@ability_cooldown\>/
syn match   swarklandDirective /@knowledge\>/
syn match   swarklandDirective /@perceived_thing\>/
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
syn keyword swarklandBareword human ogre lich pink_blob air_elemenetal dog ant bee beetle scorpion snake cobra unseen
syn keyword swarklandBareword player ai
syn keyword swarklandBareword confusion digging magic_missile speed remedy unknown
syn keyword swarklandBareword healing poison ethereal_vision cogniscopy blindness invisibility
syn keyword swarklandBareword polymorph
syn keyword swarklandBareword magic_bullet mapping force assume_form blinding slowing
syn keyword swarklandBareword spit_blinding_venom
syn keyword swarklandBareword red blue green yellow orange purple
syn keyword swarklandBareword       bone       gold       plastic       copper       purple
syn keyword swarklandBareword shiny_bone shiny_gold shiny_plastic shiny_copper shiny_purple

" patterns
syn match   swarklandNumber /-\?\<\d\{1,10\}\>/         " int
syn match   swarklandNumber /\<\x\{8\}\>/               " seed
syn match   swarklandNumber /\<\x\{64\}\>/              " id
syn match   swarklandNumber /\<\x\{2500\}\>/            " aesthetics
syn match   swarklandNumber /\<\x\{4992\}\>/            " rng_state (although my gvim seems to only color the first 3000 characaters of a line)
syn match   swarklandNumber /\<[a-zA-Z_0-9]\{1250\}\>/  " map_tiles
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
