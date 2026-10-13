"""Build an inspectable report without falsely joining independent captures."""
import argparse
import csv
import html
import importlib.util
import json
import os
from pathlib import Path


def load(name,filename):
    spec=importlib.util.spec_from_file_location(name,Path(__file__).with_name(filename))
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def esc(value):
    return html.escape(str(value),quote=True)


def plot(events):
    end=max(e['relative_ms'] for e in events) or 1
    parts=['<svg viewBox="0 0 1000 250" role="img" aria-label="Host timing in milliseconds">']
    for t in (20,40,80,120,160):
        y=210-min(t,160)
        parts.append(f'<path d="M50 {y} H980" stroke="#d7dfeb"/><text x="0" y="{y+4}">{t} ms</text>')
    for key,color in [('displayed_ms','#d5482e'),('between_presents_ms','#126799')]:
        for event in events:
            value=event[key]
            if value is not None:
                x=50+event['relative_ms']/end*930
                y=210-min(value,160)
                parts.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="1.8" fill="{color}"><title>{event["relative_ms"]:.3f} ms: {value:.3f} ms</title></circle>')
    parts.append(f'<text x="50" y="240">0 s</text><text x="860" y="240">{end/1000:.2f} s</text></svg>')
    return ''.join(parts)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host',type=Path,action='append',default=[])
    parser.add_argument('--frames',type=Path)
    parser.add_argument('--expected-step',type=int,default=6)
    parser.add_argument('--crop',type=int,nargs=4,default=[100,260,500,60])
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    if not args.host and not args.frames:
        parser.error('Provide host measurements or source images')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    host=load('host','check-host-presentation.py')
    scroll=load('scroll','check-scroll-frames.py')
    parts=['''<!doctype html><html lang="no"><meta charset="utf-8"><title>Scrollmåling</title>
<style>body{font:16px system-ui;background:#f3f6fa;color:#172b40;max-width:1100px;margin:35px auto;padding:0 25px}section{background:white;padding:24px;margin:20px 0;border-radius:12px}table{border-collapse:collapse;width:100%;font-variant-numeric:tabular-nums}th,td{padding:7px;text-align:left;border-bottom:1px solid #d7dfeb}svg{width:100%;font:12px system-ui}img{max-width:48%;image-rendering:pixelated}code{overflow-wrap:anywhere}p{line-height:1.5}.note{border-left:4px solid #c78113;padding-left:14px}</style>
<h1>Scrollmåling: bevegelse og virkelig tid</h1><p class="note">Dette er separate målinger. Ingen bildefil er koblet til en bestemt Windows-hendelse. Rapporten viser observerte avvik, ikke en ferdig årsaksdiagnose. Grafen bruker CPU-starttid langs x-aksen; tabellen viser rekonstruert visningstid der den finnes.</p>''']
    summary_data={'host':[]}
    for path in args.host:
        with path.open(encoding='utf-8-sig',newline='') as stream:
            summary,events=host.analyze(list(csv.DictReader(stream)))
        summary_data['host'].append({'file':str(path),'summary':summary})
        parts.append(f'<section><h2>Windows / PC-klokke</h2><code>{esc(path)}</code>')
        for group in summary:
            selected=[e for e in events if e['process_id']==group['process_id'] and e['swap_chain']==group['swap_chain']]
            p=group['between_presents'];d=group['display_intervals']
            def value(s,key):return f'{s[key]:.3f}' if s else 'Ikke tilgjengelig'
            parts.append(f'<p>{group["frames"]} hendelser · PID {esc(group["process_id"])} · swap chain {esc(group["swap_chain"])}</p><table><tr><th>Måling</th><th>Median ms</th><th>95-persentil ms</th><th>Maks ms</th></tr>')
            for title,data in [('Leveringsintervall',p),('Visningsintervall',d)]:
                parts.append(f'<tr><td>{title}</td><td>{value(data,"median_ms")}</td><td>{value(data,"p95_ms")}</td><td>{value(data,"max_ms")}</td></tr>')
            parts.append('</table>'+plot(selected)+'<p>Blå: levering. Oransje: visning. Verdier over 160 ms tegnes ved øvre grense; hold pekeren over punktet for full verdi.</p>')
            residual=group['display_clock_max_residual_ms']
            parts.append(f'<p>Kontroll av rekonstruert visningsklokke: maks avvik {esc(residual)} ms. Manglende/ufullstendige visningsverdier: {group["not_displayed_or_zero_or_unavailable"]}.</p>')
            parts.append('<details><summary>20 lengste observerte visningsintervaller</summary><table><tr><th>Rad (fra 0)</th><th>Visning QPC ms</th><th>Visningsintervall ms</th><th>Leveringsintervall ms</th></tr>')
            worst=sorted((e for e in selected if e['displayed_ms'] is not None),key=lambda e:e['displayed_ms'],reverse=True)[:20]
            for e in worst:
                parts.append(f'<tr><td>{e["row"]}</td><td>{esc(e["display_qpc_ms"])}</td><td>{e["displayed_ms"]:.3f}</td><td>{esc(e["between_presents_ms"])}</td></tr>')
            parts.append('</table></details>')
        parts.append('</section>')
    if args.frames:
        summary,events=scroll.analyze(sorted(args.frames.glob('*.png')),args.crop,args.expected_step)
        summary_data['content']=summary
        parts.append(f'<section><h2>Faktisk pikselbevegelse</h2><p>{summary["transitions"]} overganger: {summary["faults"]} avvik, {summary["uncertain"]} usikre. Forventet steg: {args.expected_step} skjermpiksler.</p><p>Dette opptaket gir bilderekkefølge, ikke pålitelige Windows-tidspunkter. Klassifisering: <strong>{summary["result"]}</strong>.</p>')
        for event in (e for e in events if e['kind']!='ok'):
            parts.append(f'<details open><summary>Overgang {event["transition"]}: {esc(event["kind"])} — {event["step"]} piksler</summary><p>{esc(event["previous"])} → {esc(event["current"])}</p>')
            for key in ('previous','current'):
                rel=os.path.relpath(Path(event[key]).resolve(),args.output.parent.resolve()).replace(os.sep,'/')
                parts.append(f'<a href="{esc(rel)}"><img src="{esc(rel)}" alt="{key}"></a>')
            parts.append('</details>')
        parts.append('</section>')
    parts.append('</html>')
    args.output.write_text(''.join(parts),encoding='utf-8')
    args.output.with_suffix('.json').write_text(json.dumps(summary_data,indent=2),encoding='utf-8')
    print(args.output.resolve())


if __name__=='__main__':
    main()
