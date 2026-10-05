import sys
from pypdf import PdfReader

r = PdfReader('d:/dev/prjs/mnArpee/BlueARP_Manual_v280_EN.pdf')
print('pages:', len(r.pages))
pages = [int(a) for a in sys.argv[1:]] or [1, 2, 3, 4]
for i in pages:
    print(f'--- p{i} ---')
    print((r.pages[i].extract_text() or '')[:2500])
