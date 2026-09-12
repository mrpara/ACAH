import zipfile, os
z = zipfile.ZipFile('wave14_src.zip')
n_ok = 0
for n in z.namelist():
    if n.endswith('/'): continue
    os.makedirs(os.path.dirname(n) or '.', exist_ok=True)
    with open(n, 'wb') as f: f.write(z.read(n))
    n_ok += 1
print('unpacked', n_ok, 'files')
