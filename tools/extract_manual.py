from pypdf import PdfReader

r = PdfReader('d:/dev/prjs/mnArpee/BlueARP_Manual_v280_EN.pdf')
full = []
for i, p in enumerate(r.pages):
    full.append(f'\n===== PDF-PAGE {i} =====\n' + (p.extract_text() or ''))
text = '\n'.join(full)
with open('d:/dev/prjs/mnArpee/tools/manual.txt', 'w', encoding='utf-8') as f:
    f.write(text)
print('chars:', len(text))
