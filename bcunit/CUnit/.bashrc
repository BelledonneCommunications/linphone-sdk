# ~/.bashrc: executed by bash(1) for non-login shells.
# see /usr/share/doc/bash/examples/startup-files for examples

# If running interactively, then:
if [ "$PS1" ]; then

    # enable color support of ls and also add handy aliases

    eval `dircolors`
    alias ls='ls --color=auto '
    #alias ll='ls -l'
    #alias la='ls -A'
    #alias l='ls -CF'
    #alias dir='ls --color=auto --format=vertical'
    #alias vdir='ls --color=auto --format=long'

    # set a fancy prompt

    PS1='\u@\h:\w\$ '
fi
