import re
f = open('./fs/fs.c','r')
contents = f.read()
pattern = r'[a-zA-Z0-9_]+\s[a-zA-Z0-9_]+\(.+\)(\s*){'
what = r'file_free_block'
pat = re.compile(what)

for thing in pat.findall(contents):
	print thing
