project('linphone-sdk', 'c', version : '4.4.0')

#subdir('bctoolbox')

ortp_dep = dependency('ortp', fallback : ['ortp', 'ortp_dep'])

executable('dummy', 'dummy.c', dependencies : ortp_dep)


