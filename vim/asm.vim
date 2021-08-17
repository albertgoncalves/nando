" $ cp asm.vim ~/.vim/syntax/asm.vim
" $ grep '.asm' ~/.vimrc
" autocmd BufNewFile,BufRead *.asm setlocal filetype=asm

syn match Comment   "//.*$"
syn match Delimiter "[()@=;+\-!&|]"
syn match Number    "\<[0-9]\+\>"
