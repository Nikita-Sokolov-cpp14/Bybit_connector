#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
decoder_PFD.py — валидация индикатора Price-Flow Divergence (comb_W360)
по живому логу стратегии log_files/price_flow_divergence.csv.

Лог содержит готовые фичи индикатора на минутных барах (после прогревного окна):
  ts, mid_price, pz, intz, comb, tr, sig_px, std_flow, signal
  signal: 0=Unknown, 1=Buy, 2=Sell (Side_Unknown=0, Side_Buy=1, Side_Sell=2).

Что проверяется (без заглядывания вперёд):
  - Spearman-корреляция comb/pz/intz/tr с форвард-доходностью на H = {1ч, 3ч, 6ч};
  - base rate: доля up-move и вероятность достижения цели раньше стопа "всегда в рынке";
  - пороговый анализ сигнала comb: long (comb > +th), short (comb < -th),
    двухсторонний (long | short), с целью/стопом/комиссиями;
  - как реально сработали сигналы, записанные стратегией в колонку signal (sigma=2.5);
  - grid-search порога на train (по времени), верификация EV на test и по дням.

Использование (из decoder/):
    python3 decoder_PFD.py
    python3 decoder_PFD.py --file ../log_files/price_flow_divergence.csv --tp 0.005 --sl 0.0025

Выход: pfd_dashboard.html (+ pfd_*.html) и печать таблиц.
"""

import argparse
import glob
import os
import sys
import warnings

import numpy as np
import pandas as pd
from scipy import stats

warnings.filterwarnings('ignore')

DEFAULT_DATA_DIR = '../log_files'
DEFAULT_OUT_DIR = '.'
DEFAULT_FILE = 'price_flow_divergence.csv'

HORIZONS_MIN = (60, 180, 360)          # форвард-горизонты (мин): 1ч, 3ч, 6ч
TARGETS = (0.0025, 0.0050, 0.0075)     # цели (доли)
SLS = (0.0010, 0.0025, 0.0050)         # сетка стопов
SL = 0.0025                            # стоп по умолчанию (0.25%)
TP_DEFAULT = 0.0050                    # цель по умолчанию (0.5%)
FEES_TAKER = 0.0014                    # круговая комиссия, taker (0.14%)
FEES_MAKER = 0.0004                    # круговая комиссия, maker (0.04%)
TRAIN_RATIO = 0.65                     # доля временного сплита train/test

COLUMNS = ['ts', 'mid_price', 'pz', 'intz', 'comb', 'tr', 'sig_px', 'std_flow', 'signal']


# ----------------------------------------------------------------------------
# Загрузка
# ----------------------------------------------------------------------------
def load_all(data_dir, fname):
    paths = sorted(glob.glob(os.path.join(data_dir, '**', fname), recursive=True))
    paths += [os.path.join(data_dir, fname)]
    frames = []
    for p in paths:
        if not os.path.exists(p):
            continue
        try:
            df = pd.read_csv(p, delimiter='\t')
        except Exception as e:
            print(f'  [skip] {p}: {e}')
            continue
        df = df[[c for c in COLUMNS if c in df.columns]]
        frames.append(df)
    if not frames:
        sys.exit('Файлы не найдены')
    df = pd.concat(frames, ignore_index=True)
    df = df.dropna(subset=['ts', 'mid_price'])
    df['ts'] = df['ts'].astype('int64')
    df.drop_duplicates(subset=['ts'], keep='last', inplace=True)
    df.sort_values('ts', inplace=True)
    df.reset_index(drop=True, inplace=True)
    df['dt'] = pd.to_datetime(df['ts'], unit='ms')
    return df


# ----------------------------------------------------------------------------
# Метки: форвард-доходность и TP/SL
# ----------------------------------------------------------------------------
def add_returns(features, horizons_min):
    px = features['mid_price'].to_numpy()
    n = len(px)
    for H in horizons_min:
        ret = np.full(n, np.nan)
        ret[:-H] = px[H:] / px[:-H] - 1.0
        features[f'ret_H{H}'] = ret


def add_hit_cols(features, H, c, sl):
    """Цель c была достигнута раньше стопа sl внутри H минут после бара."""
    px = features['mid_price'].to_numpy()
    n = len(px)
    Hb = int(H)
    hit_buy = np.zeros(n, dtype=bool)
    hit_sell = np.zeros(n, dtype=bool)
    for i in range(n - Hb):
        seg = px[i + 1:i + Hb + 1]
        if len(seg) == 0:
            continue
        hi, lo, p = seg.max(), seg.min(), px[i]
        hit_buy[i] = (lo > p * (1 - sl)) and (hi >= p * (1 + c))
        hit_sell[i] = (hi < p * (1 + sl)) and (lo <= p * (1 - c))
    tag = f'H{H}_{int(c * 1e4)}_{int(sl * 1e4)}'
    features[f'hit_buy_{tag}'] = hit_buy
    features[f'hit_sell_{tag}'] = hit_sell


def base_rate(features, H, c, sl):
    col = f'ret_H{H}'
    add_hit_cols(features, H, c, sl)
    tag = f'H{H}_{int(c * 1e4)}_{int(sl * 1e4)}'
    d = features[[col, f'hit_buy_{tag}', f'hit_sell_{tag}']].dropna()
    return {
        'up_share': (d[col] > 0).mean(),
        'tp_buy': d[f'hit_buy_{tag}'].mean(),
        'tp_sell': d[f'hit_sell_{tag}'].mean(),
    }


# ----------------------------------------------------------------------------
# Корреляции
# ----------------------------------------------------------------------------
def analyze_corr(features, horizons_min):
    rows = []
    for feat in ('pz', 'intz', 'comb', 'tr'):
        for H in horizons_min:
            col = f'ret_H{H}'
            d = features[[feat, col]].dropna()
            if len(d) < 200:
                continue
            rho, pv = stats.spearmanr(d[feat], d[col])
            rows.append({'feature': feat, 'H_min': H, 'rho': rho,
                         'p_value': pv, 'n': len(d)})
    return pd.DataFrame(rows)


# ----------------------------------------------------------------------------
# Пороговый анализ одного направления
# ----------------------------------------------------------------------------
def ev_series(features, H, c, sl, side, fees, ths):
    """Для сетки порогов th>0: long если comb>th, short если comb<-th."""
    px = features['mid_price'].to_numpy()
    comb = features['comb'].to_numpy()
    n = len(px)
    Hb = int(H)
    tag = f'H{H}_{int(c * 1e4)}_{int(sl * 1e4)}'
    if f'hit_buy_{tag}' not in features.columns:
        add_hit_cols(features, H, c, sl)
    hit_long = features[f'hit_buy_{tag}'].to_numpy()
    hit_short = features[f'hit_sell_{tag}'].to_numpy()
    ret = features[f'ret_H{H}'].to_numpy()

    rows = []
    for th in ths:
        if th <= 0:
            continue
        if side == 'long':
            sel = comb > th
            hit = hit_long
            direct = (ret > 0)
        elif side == 'short':
            sel = comb < -th
            hit = hit_short
            direct = (ret < 0)
        else:  # two-sided
            sel = (comb > th) | (comb < -th)
            hit = np.where(comb > th, hit_long, hit_short)
            direct = np.where(comb > th, ret > 0, ret < 0)
        k = int(sel.sum())
        if k < 30:
            continue
        h = int((hit[sel] & ~np.isnan(ret[sel])).sum())
        tp_rate = h / k
        ev = (tp_rate * c - (1 - tp_rate) * sl) - fees
        rows.append({'th': th, 'n': k, 'tp_rate': tp_rate,
                     'direct': direct[sel].mean(), 'ev': ev})
    return pd.DataFrame(rows)


def threshold_report(features, H, c, sl, fees):
    ths = np.unique(np.nanquantile(np.abs(features['comb']),
                                   np.arange(0.0, 1.05, 0.05)))
    out = {}
    for side in ('long', 'short', 'two'):
        tb = ev_series(features, H, c, sl, side, fees, ths)
        out[side] = tb
        if len(tb):
            b = tb.loc[tb['ev'].idxmax()]
            print(f'  {side:5s} EV={b["ev"]*1e4:+.2f} бп  n={b["n"]:5.0f}  '
                  f'порог={b["th"]:.2f}  TP-доля={b["tp_rate"]:.1%}  '
                  f'направл.={b["direct"]:.1%}')
    return out


# ----------------------------------------------------------------------------
# Grid + train/test + стабильность по дням
# ----------------------------------------------------------------------------
def grid_search(train, H, c, sl, fees):
    ths = np.unique(np.nanquantile(np.abs(train['comb']), np.arange(0.0, 1.05, 0.05)))
    rows = []
    for side in ('long', 'short', 'two'):
        tb = ev_series(train, H, c, sl, side, fees, ths)
        if not len(tb):
            continue
        b = tb.loc[tb['ev'].idxmax()]
        rows.append({'H': H, 'side': side, 'th': b['th'], 'n': b['n'],
                     'tp_rate': b['tp_rate'], 'direct': b['direct'], 'ev': b['ev']})
    return pd.DataFrame(rows)


def eval_cfg(features, cfg, c, sl, fees):
    ths = [cfg['th']]
    tb = ev_series(features, cfg['H'], c, sl, cfg['side'], fees, ths)
    if not len(tb):
        return None
    return tb.iloc[0]


def per_day(features, cfg, c, sl, fees):
    out = []
    for dt, g in features.groupby(features['dt'].dt.date):
        r = eval_cfg(g, cfg, c, sl, fees)
        if r is None:
            continue
        out.append({'day': str(dt), 'n': r['n'], 'tp_rate': r['tp_rate'],
                    'direct': r['direct'], 'ev': r['ev']})
    return pd.DataFrame(out)


# ----------------------------------------------------------------------------
# Сигналы, записанные стратегией (колонка signal)
# ----------------------------------------------------------------------------
def live_signals(features, H, c, sl, fees):
    """Оценка сигналов, реально выставленных стратегией (signal=1/2)."""
    tag = f'H{H}_{int(c * 1e4)}_{int(sl * 1e4)}'
    if f'hit_buy_{tag}' not in features.columns:
        add_hit_cols(features, H, c, sl)
    ret = features[f'ret_H{H}']
    buy = features['signal'] == 1
    sell = features['signal'] == 2
    report = {}
    for name, sel, hit, drc in (
            ('BUY', buy, features[f'hit_buy_{tag}'], ret > 0),
            ('SELL', sell, features[f'hit_sell_{tag}'], ret < 0)):
        k = int(sel.sum())
        if k == 0:
            report[name] = None
            continue
        h = int(hit[sel].sum())
        tp_rate = h / k
        ev = (tp_rate * c - (1 - tp_rate) * sl) - fees
        report[name] = {'n': k, 'tp_rate': tp_rate,
                        'direct': drc[sel].mean(), 'ev': ev}
        print(f'  {name}: n={k}  TP-доля={tp_rate:.1%}  '
              f'направл.={drc[sel].mean():.1%}  EV={ev*1e4:+.2f} бп')
    return report


# ----------------------------------------------------------------------------
# HTML-отчёт
# ----------------------------------------------------------------------------
def build_dashboard(features, out_dir, corr, evals, base, H, c, sl,
                    grid, best, te_ev, pdays, live):
    import plotly.graph_objects as go
    from plotly.subplots import make_subplots

    buffered = features.copy()
    sold = c * 1e4

    # 1) Цена + comb + живые сигналы
    fig1 = go.Figure()
    fig1.add_trace(go.Scatter(x=features['dt'], y=features['mid_price'], mode='lines',
                              name='Цена', line=dict(color='black', width=1.3)))
    fig1.add_trace(go.Scatter(x=features['dt'], y=features['comb'], mode='lines',
                              name='comb', yaxis='y2', line=dict(color='steelblue', width=1.1)))
    buy = features['signal'] == 1
    sell = features['signal'] == 2
    fig1.add_trace(go.Scatter(x=features['dt'][buy], y=features['mid_price'][buy],
                              mode='markers', name='лого live BUY',
                              marker=dict(color='green', size=6, symbol='triangle-up'), yaxis='y'))
    fig1.add_trace(go.Scatter(x=features['dt'][sell], y=features['mid_price'][sell],
                              mode='markers', name='лого live SELL',
                              marker=dict(color='red', size=6, symbol='triangle-down'), yaxis='y'))
    fig1.update_layout(title='<b>Цена и comb (PFD), живые сигналы</b>', height=520,
                       xaxis_title='Время', yaxis=dict(title='Цена'),
                       yaxis2=dict(title='comb', overlaying='y', side='right', showgrid=False),
                       hovermode='x unified', dragmode='pan')
    fig1.write_html(os.path.join(out_dir, 'pfd_price.html'))

    # 2) Корреляционная матрица
    fig2 = go.Figure(go.Heatmap(
        z=corr.pivot_table(index='feature', columns='H_min', values='rho'),
        x=[f'{h} мин' for h in corr['H_min'].unique()],
        y=corr['feature'].unique(), colorscale='RdBu', zmid=0, zmin=-0.2, zmax=0.2,
        text=np.round(corr.pivot_table(index='feature', columns='H_min', values='rho').values, 3),
        texttemplate='%{text}', textfont=dict(size=9),
        hovertemplate='фича: %{y}<br>горизонт: %{x}<br>rho=%{z:.4f}<extra></extra>'))
    fig2.update_layout(title='<b>Корреляция Spearman: фича vs форвард-движение</b>',
                       height=380, yaxis_autorange='reversed')
    fig2.write_html(os.path.join(out_dir, 'pfd_correlation.html'))

    # 3) Кривые EV/точности по порогам
    fig3 = make_subplots(rows=1, cols=2, subplot_titles=('TP-доля и направление',
                                                         'EV сделки'))
    used = False
    for side, label, col in (('long', 'long', '#2e7d32'), ('short', 'short', '#c62828'),
                             ('two', 'two', '#1565c0')):
        tb = evals.get(side)
        if tb is None or len(tb) == 0:
            continue
        used = True
        fig3.add_trace(go.Scatter(x=tb['n'], y=tb['tp_rate'], mode='lines+markers',
                                  name=f'TP-доля {label}', line=dict(color=col)), row=1, col=1)
        fig3.add_trace(go.Scatter(x=tb['n'], y=tb['direct'], mode='lines+markers',
                                  name=f'направл. {label}', line=dict(color=col, dash='dot')),
                       row=1, col=1)
        fig3.add_trace(go.Scatter(x=tb['n'], y=tb['ev'] * 1e4, mode='lines+markers',
                                  name=f'EV {label}', line=dict(color=col)), row=1, col=2)
    if used:
        fig3.add_hline(y=0, line_color='gray', line_width=1, row=1, col=2)
        fig3.update_xaxes(title_text='число сигналов (жёстче порог)', type='log')
        fig3.update_layout(title=f'<b>Сигнал по порогам (TP {c:.2%}, SL {sl:.2%}, H={H} мин)</b>',
                           height=480)
        fig3.write_html(os.path.join(out_dir, 'pfd_curves.html'))

    # 4) Heatmap EV по TP x SL на тесте
    split = int(len(features) * TRAIN_RATIO)
    tr_f, te_f = features.iloc[:split], features.iloc[split:]
    side = best['side']
    feat_th = best['th']
    zmat = np.full((len(SLS), len(TARGETS)), np.nan)
    for j, tpc in enumerate(TARGETS):
        for i, slc in enumerate(SLS):
            cfg = dict(H=H, side=side, th=feat_th)
            r = eval_cfg(te_f, cfg, tpc, slc, FEES_TAKER)
            if r is not None:
                zmat[i, j] = r['ev'] * 1e4
    fig4 = go.Figure(go.Heatmap(
        z=zmat, x=[f'TP {tpc:.2%}' for tpc in TARGETS],
        y=[f'SL {slc:.2%}' for slc in SLS],
        colorscale='RdYlGn', zmid=0,
        text=np.round(zmat, 1), texttemplate='%{text}', textfont=dict(size=10),
        hovertemplate='TP: %{x}<br>SL: %{y}<br>EV тест: %{z:.1f} бп<extra></extra>'))
    fig4.update_layout(title=f'<b>EV на TEST (бп/сделку): {side}, порог={feat_th:.2f}, H={H} мин</b>',
                       height=400)
    fig4.write_html(os.path.join(out_dir, 'pfd_heatmap.html'))

    # 5) EV по дням на тесте
    fig5 = go.Figure()
    if pdays is not None and len(pdays):
        fig5.add_trace(go.Bar(x=pdays['day'], y=pdays['ev'] * 1e4,
                              marker_color=['#2e7d32' if v > 0 else '#c62828'
                                            for v in pdays['ev']],
                              hovertemplate='день: %{x}<br>EV=%{y:.1f} бп<br>n=%{customdata:.0f}'
                                            '<extra></extra>',
                              customdata=pdays['n']))
        fig5.add_hline(y=0, line_color='gray', line_width=1)
        fig5.update_layout(title=f'<b>EV по дням на TEST ({side}, порог={feat_th:.2f})</b>',
                           height=320, yaxis_title='EV, бп/сделку')
        fig5.write_html(os.path.join(out_dir, 'pfd_days.html'))

    # 6) Сводный HTML
    base_tp = base['tp_buy'] if base['tp_buy'] >= base['tp_sell'] else base['tp_sell']
    live_buy = live.get('BUY') if live else None
    html = f'''<!DOCTYPE html>
<html><head><title>PFD Анализ</title><style>
 body {{ font-family:'Segoe UI',Arial,sans-serif; background:#f0f2f5; margin:0; padding:20px; }}
 h1 {{ color:#1a1a2e; text-align:center; font-size:26px; }}
 .subtitle {{ text-align:center; color:#666; font-size:14px; margin-bottom:20px; }}
 .stats {{ background:white; border-radius:12px; padding:20px; margin:20px 0; box-shadow:0 4px 20px rgba(0,0,0,0.08); }}
 .graph {{ background:white; border-radius:12px; margin:20px 0; padding:16px; box-shadow:0 4px 20px rgba(0,0,0,0.08); }}
 .graph iframe {{ width:100%; border:none; border-radius:8px; }}
</style></head><body>
<h1>📊 Price-Flow Divergence — анализ лога</h1>
<p class="subtitle">Характеристики сигнала comb (было: цена обгоняет поток) по данным {features['dt'].min():%d.%m.%Y} – {features['dt'].max():%d.%m.%Y}</p>
<div class="stats"><table><tr><td>Минутных баров</td><td>{len(features):,}</td></tr>
<tr><td>Период записи</td><td>{features['dt'].min()} – {features['dt'].max()}</td></tr>
<tr><td>Диапазон цены</td><td>{features['mid_price'].min():.0f} – {features['mid_price'].max():.0f}</td></tr>
<tr><td>Base up-move ({H//60}ч)</td><td>{base['up_share']:.1%}</td></tr>
<tr><td>Base TP-доля (TP {c:.2%}, SL {sl:.2%})</td><td>{base_tp:.1%}</td></tr>
<tr><td>Лучший конфиг (train choice)</td><td>{best['side']}, th={best['th']:.2f}</td></tr>
<tr><td>EV на test</td><td>{((te_ev['ev'] * 1e4) if te_ev is not None else float('nan')):.1f} бп</td></tr>
<tr><td>Живые сигналы BUY</td><td>{(live_buy['n'] if live_buy else 0)} ({("%.1f%%" % (live_buy['tp_rate'] * 100)) if live_buy and live_buy['n'] else '-'})</td></tr></table></div>
<div class="graph"><h3>📈 Цена + comb и живые сигналы</h3><iframe src="pfd_price.html" height="540"></iframe></div>
<div class="graph"><h3>🌡 Корреляция с форвард-движением</h3><iframe src="pfd_correlation.html" height="400"></iframe></div>
<div class="graph"><h3>💰 Кривые EV/точности по порогам</h3><iframe src="pfd_curves.html" height="500"></iframe></div>
<div class="graph"><h3>🗺 EV на тесте по сетке TP×SL</h3><iframe src="pfd_heatmap.html" height="420"></iframe></div>
<div class="graph"><h3>📅 EV по дням на тесте</h3><iframe src="pfd_days.html" height="340"></iframe></div>
</body></html>'''
    with open(os.path.join(out_dir, 'pfd_dashboard.html'), 'w') as f:
        f.write(html)
    print(f'  HTML: pfd_dashboard.html (+ pfd_*.html) в {out_dir}')


# ----------------------------------------------------------------------------
# main
# ----------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--data', default=DEFAULT_DATA_DIR)
    ap.add_argument('--out', default=DEFAULT_OUT_DIR)
    ap.add_argument('--file', default=DEFAULT_FILE)
    ap.add_argument('--horizons', type=str, default='')
    ap.add_argument('--tp', type=float, default=TP_DEFAULT)
    ap.add_argument('--sl', type=float, default=SL)
    ap.add_argument('--fee', type=float, default=FEES_TAKER)
    args = ap.parse_args()

    horizons = tuple(int(x) for x in args.horizons.split(',')) if args.horizons \
        else HORIZONS_MIN

    print('=' * 66)
    print('DECODER PFD — Price-Flow Divergence (comb_W360)')
    print('=' * 66)

    if args.file == DEFAULT_FILE:
        data = os.path.join(args.data, args.file)
    else:
        data = args.file if os.path.exists(args.file) else \
            os.path.join(args.data, args.file)

    print(f'\nЗагрузка: {data}')
    features = load_all(os.path.dirname(data) or '.', os.path.basename(data))
    print(f'  Баров: {len(features):,} ({features["dt"].min()} .. {features["dt"].max()})')

    add_returns(features, horizons)
    print(f'\nКорреляция comb с форвард-доходностью: '
          f'{features["comb"].corr(features["ret_H360"]) if 360 in horizons else float("nan"):.3f} '
          f'(6ч)')

    print('\n' + '-' * 66)
    print('КОРРЕЛЯЦИЯ С ФОРВАРД-ДВИЖЕНИЕМ (Spearman)')
    print('-' * 66)
    corr = analyze_corr(features, horizons)
    print(corr.to_string(index=False))

    print('\n' + '-' * 66)
    print(f'BASE RATE (всегда в рынке)')
    print('-' * 66)
    base = base_rate(features, HORIZONS_MIN[2], args.tp, args.sl)
    print(f'  {HORIZONS_MIN[2]//60}ч: доля up-move = {base["up_share"]:.1%}; '
          f'TP-доля buy={base["tp_buy"]:.1%}, sell={base["tp_sell"]:.1%} '
          f'(TP {args.tp:.2%}, SL {args.sl:.2%})')

    print('\n' + '-' * 66)
    print(f'ПОРОГОВЫЙ АНАЛИЗ comb (H={HORIZONS_MIN[2]} мин, TP {args.tp:.2%}, '
          f'SL {args.sl:.2%}, комиссия {args.fee:.2%})')
    print('  comb > +th -> long; comb < -th -> short; two = оба направления')
    print('-' * 66)
    evals = threshold_report(features, HORIZONS_MIN[2], args.tp, args.sl, args.fee)

    print('\n' + '-' * 66)
    print('ЖИВЫЕ СИГНАЛЫ СТРАТЕГИИ (колонка signal, sigma=2.5)')
    print('-' * 66)
    live = live_signals(features, HORIZONS_MIN[2], args.tp, args.sl, args.fee)

    print('\n' + '-' * 66)
    print('GRID-SEARCH НА TRAIN + ВЕРИФИКАЦИЯ НА TEST')
    print('-' * 66)
    split = int(len(features) * TRAIN_RATIO)
    tr = features.iloc[:split].reset_index(drop=True)
    te = features.iloc[split:].reset_index(drop=True)
    print(f'  train: {len(tr)} баров, test: {len(te)} баров')
    grid = grid_search(tr, HORIZONS_MIN[2], args.tp, args.sl, args.fee)
    grid = grid.sort_values('ev', ascending=False)
    print('  Топ по EV (выбор на TRAIN):')
    print('  ' + grid.to_string(index=False))
    best = grid.iloc[0].to_dict()
    tr_ev = eval_cfg(tr, best, args.tp, args.sl, args.fee)
    te_ev = eval_cfg(te, best, args.tp, args.sl, args.fee)
    print(f'\n  Лучший конфиг: {best["side"]}, th={best["th"]:.2f}')
    if tr_ev is not None and te_ev is not None:
        print(f'  EV train = {tr_ev["ev"]*1e4:+.2f} бп (n={tr_ev["n"]:.0f}); '
              f'EV TEST = {te_ev["ev"]*1e4:+.2f} бп (n={te_ev["n"]:.0f}); '
              f'test TP-доля = {te_ev["tp_rate"]:.1%}')
        pdays = per_day(te, best, args.tp, args.sl, args.fee)
        if len(pdays):
            pos = (pdays['ev'] > 0).sum()
            print(f'  Дней с EV>0: {pos}/{len(pdays)}; суммарный EV={pdays["ev"].sum()*1e4:+.1f} бп')
            print('  ' + pdays.to_string(index=False))
    else:
        te_ev = None
        pdays = None

    print('\n' + '-' * 66)
    print('ГЕНЕРАЦИЯ HTML')
    print('-' * 66)
    build_dashboard(features, args.out, corr, evals, base, HORIZONS_MIN[2],
                    args.tp, args.sl, grid, best, te_ev, pdays, live)


if __name__ == '__main__':
    main()