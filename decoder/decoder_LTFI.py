#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
decoder_LTFI.py — валидация "длинного" TFI (Long Trade Flow Indicator).

Идея: текущий TFI агрегирует поток за 3–15 минут (микроструктура) и не годится
для прогноза движения на часы. Здесь строится часовой аналог из того же лога:

  - из пер-секундного ряда netFlow собираются минутные бары;
  - на окнах W = {30м, 1ч, 3ч} считаются следующие фичи (без заглядывания вперёд):
      * int_W     — интеграл netFlow за окно (прокси наклон/знак CVD);
      * intz_W    — тот же интеграл, нормированный на волатильность потока (t-статистика);
      * z5_W      — z-score короткого (5 мин) потока против длинного окна;
      * slope_W   — средний поток за окно (наклон линии накопленного потока);
      * div_W     — дивергенция поток/цена: поток тянет, а цена ещё не ушла;
      * vol_W     — волатильность потока (режимный фильтр);
  - метка: форвард-доходность на горизонтах H = {1ч, 3ч, 6ч, 12ч} (+ вероятность
    движения >= c c целевым стопом).

ВАЖНОЕ ОГРАНИЧЕНИЕ: в логе хранится нормированный netFlow = (Vbuy - Vsell)/(Vbuy + Vsell)
без объёмов, поэтому точный CVD (объёмный) тут невосстановим — интеграл netFlow — это
прокси направления/силы потока. Для объёмного CVD нужно логировать объёмы.

Использование (из decoder/):
    python3 decoder_LTFI.py
    python3 decoder_LTFI.py --data ../log_files --out . --horizons 360

Выход: ltfi_dashboard.html (+ ltfi_*.html) и печать таблиц корреляции/точности.
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

# ----------------------------------------------------------------------------
# Параметры
# ----------------------------------------------------------------------------
DEFAULT_DATA_DIR = '../log_files'
DEFAULT_OUT_DIR = '.'
TRADEFILE = 'trade_flow.csv'

BAR_SEC = 60                     # минутный бар
HORIZONS_MIN = (60, 180, 360, 720)   # форвард-горизонты (мин): 1ч,3ч,6ч,12ч
WINDOWS_MIN = (30, 180, 360)         # окна накопления потока (мин)
TARGETS = (0.0020, 0.0035, 0.0050, 0.0075)   # целевые движения (доли)
SL = 0.0025                            # стоп для TP/SL-симуляции (в долях)
FEES_MAKER = 0.0004                    # круговая комиссия, maker (0.04%)
FEES_TAKER = 0.0014                    # круговая комиссия, taker (0.14%)
SLS = (0.0015, 0.0025, 0.0035, 0.0050)   # сетка стопов для grid-search
TRAIN_RATIO = 0.65                     # доля временного сплита train/test

COLUMNS = ['ts', 'mid_price', 'netFlow', 'mu', 'sigma', 'z_score',
           'signal', 'shortWin', 'baseWin', 'dataSize']


# ----------------------------------------------------------------------------
# Загрузка и предобработка
# ----------------------------------------------------------------------------
def load_all(data_dir):
    paths = sorted(glob.glob(os.path.join(data_dir, '**', TRADEFILE), recursive=True))
    paths += [os.path.join(data_dir, TRADEFILE)]
    frames = []
    for p in paths:
        if not os.path.exists(p):
            continue
        try:
            df = pd.read_csv(p, delimiter='\t', header=None, names=COLUMNS,
                             skiprows=1, usecols=[0, 1, 2])
        except Exception as e:
            print(f'  [skip] {p}: {e}')
            continue
        frames.append(df)
    df = pd.concat(frames, ignore_index=True)
    df.drop_duplicates(subset=['ts'], keep='last', inplace=True)
    df.sort_values('ts', inplace=True)
    df.reset_index(drop=True, inplace=True)
    return df


def build_minute_bars(df):
    df['bar'] = df['ts'] // (BAR_SEC * 1000)
    bars = df.groupby('bar').agg(
        ts=('ts', 'last'),
        price=('mid_price', 'last'),
        nf_sum=('netFlow', 'sum'),
        nf_mean=('netFlow', 'mean'),
    ).reset_index()
    bars['dt'] = pd.to_datetime(bars['ts'], unit='ms')
    return bars


# ----------------------------------------------------------------------------
# Фичи и метки (без заглядывания вперёд)
# ----------------------------------------------------------------------------
def build_features(bars, windows_min):
    n = len(bars)
    out = pd.DataFrame({'ts': bars['ts'], 'price': bars['price'], 'dt': bars['dt']})

    nf = bars['nf_sum']           # поток в баре
    px = bars['price']
    for W in windows_min:
        Wb = W  # бары == минуты
        name = f'W{W}'
        s = f'int_{name}'
        out[s] = nf.rolling(Wb, min_periods=Wb // 2).sum()
        # БАЗОВАЯ линия: простое движение цены за то же окно (моментум).
        # Если корреляция потока не превосходит её — индикатор не добавляет ценности.
        out[f'tr_{name}'] = px / px.shift(Wb) - 1.0
        # нормированный интеграл: / (std потока * sqrt(W)) ~ t-статистика среднего потока
        std_flow = nf.rolling(Wb, min_periods=Wb // 2).std(ddof=0)
        out[f'intz_{name}'] = out[s] / (std_flow * np.sqrt(Wb))
        # z-score короткого (5 мин) потока против длинного окна
        short5 = nf.rolling(5, min_periods=5).mean()
        muW = nf.rolling(Wb, min_periods=Wb // 2).mean()
        sigW = nf.rolling(Wb, min_periods=Wb // 2).std(ddof=0)
        out[f'z5_{name}'] = (short5 - muW) / sigW.replace(0, np.nan)
        # наклон накопленного потока (средний поток в баре) и его нормализация
        out[f'slope_{name}'] = nf.rolling(Wb, min_periods=Wb // 2).mean()
        # дивергенция поток/цена: z_flow минус z_price
        pzW = (px - px.rolling(Wb, min_periods=Wb // 2).mean()) / \
              px.rolling(Wb, min_periods=Wb // 2).std(ddof=0).replace(0, np.nan)
        out[f'div_{name}'] = out[f'intz_{name}'] - pzW
        # КОМБО-сигнал (дивергенция поток/цена, предсказывает будущее движение):
        #   эмпирика: форвард ≈ +tr (ценовой моментум) − intz (поток, контрариан)
        #   => score = pz (норм. движение цены) − intz (норм. поток).
        r1 = px.pct_change()
        sig_px = r1.rolling(Wb, min_periods=Wb // 2).std(ddof=0) * np.sqrt(Wb)
        out[f'pz_{name}'] = (out[f'tr_{name}'] / sig_px.replace(0, np.nan))
        out[f'comb_{name}'] = out[f'pz_{name}'] - out[f'intz_{name}']
        # режимная волатильность потока
        out[f'vol_{name}'] = std_flow

    return out


def build_targets(features, horizons_min, targets, sl):
    px = features['price'].to_numpy()
    n = len(px)
    for H in horizons_min:
        Hb = H  # бары
        name = f'H{H}'
        # форвард-доходность
        ret = np.full(n, np.nan)
        ret[:-Hb] = px[Hb:] / px[:-Hb] - 1.0
        features[f'ret_{name}'] = ret
        # индикаторы достижения цели/стопа на минутных барах внутри горизонта
        for c in targets:
            hit = np.full(n, np.nan, dtype=bool)
            hit_after = np.full(n, np.nan, dtype=bool)  # без стопа
            hit_s = np.full(n, np.nan, dtype=bool)      # SHORT: цена падает
            hit_s_after = np.full(n, np.nan, dtype=bool)
            for i in range(n - Hb):
                seg = px[i + 1:i + Hb + 1]
                if len(seg) == 0:
                    continue
                hi = seg.max()
                lo = seg.min()
                # стоп сработал раньше цели
                if lo <= px[i] * (1 - sl):
                    hit[i] = False
                elif hi >= px[i] * (1 + c):
                    hit[i] = True
                else:
                    hit[i] = False
                hit_after[i] = (hi - px[i]) / px[i] >= c
                # SHORT: цель вниз, стоп вверх (отражённая логика)
                if hi >= px[i] * (1 + sl):
                    hit_s[i] = False
                elif lo <= px[i] * (1 - c):
                    hit_s[i] = True
                else:
                    hit_s[i] = False
                hit_s_after[i] = (px[i] - lo) / px[i] >= c
            features[f'hit_{name}_{int(c * 1e4)}'] = hit
            features[f'hita_{name}_{int(c * 1e4)}'] = hit_after
            features[f'hits_{name}_{int(c * 1e4)}'] = hit_s
            features[f'hitsa_{name}_{int(c * 1e4)}'] = hit_s_after
    return features


# ----------------------------------------------------------------------------
# Анализ
# ----------------------------------------------------------------------------
def analyze(data, name, features, targets, horizons_min):
    rows = []
    for W in WINDOWS_MIN:
        for fbase in (f'intz_W{W}', f'tr_W{W}', f'comb_W{W}'):
            for H in horizons_min:
                col = f'ret_H{H}'
                d = features[[fbase, col]].dropna()
                if len(d) < 200:
                    continue
                rho, pv = stats.spearmanr(d[fbase], d[col])
                rows.append({'feature': fbase, 'W_min': W, 'H_min': H,
                             'rho': rho, 'p_value': pv, 'n': len(d)})
    return pd.DataFrame(rows)


def per_day_consistency(data, name, features, horizons_min):
    """Spearman по дням для лучшей фичи: стабильность знака = не переобучаемость"""
    res = []
    best = 'intz_W180'
    for H in (180, 360):
        col = f'ret_H{H}'
        for dt, g in features.groupby(features['dt'].dt.date):
            d = g[[best, col]].dropna()
            if len(d) < 200:
                continue
            rho, pv = stats.spearmanr(d[best], d[col])
            res.append({'day': str(dt), 'H_min': H, 'rho': rho, 'p': pv, 'n': len(d)})
    return pd.DataFrame(res)


def threshold_eval(data, name, features, feat, H, c, sl, fees, side='long',
                   lows=None):
    """Сигнал: side=long -> feat > theta (лонг), side=short -> feat < theta (шорт).
    Эмуляция TP(c)/SL через прогулку по минутным барам внутри H."""
    col_ret = f'ret_H{H}'
    if side == 'long':
        col_hit = f'hit_H{H}_{int(c * 1e4)}'
    else:
        col_hit = f'hits_H{H}_{int(c * 1e4)}'
    d = features[[feat, col_ret, col_hit, 'price']].dropna().copy()
    if lows is None:
        lows = np.nanquantile(d[feat], np.arange(0.0, 1.05, 0.05))
    out = []
    for th in sorted(lows):
        sel = d[d[feat] > th] if side == 'long' else d[d[feat] < th]
        if len(sel) < 30:
            continue
        win = sel[col_hit].mean()                      # TP-доля (со стопом)
        direct = (sel[col_ret] > 0).mean() if side == 'long' \
            else (sel[col_ret] < 0).mean()
        tp = sel[col_hit].sum()
        lss = len(sel) - tp
        gross = tp * c - lss * sl
        net = gross - len(sel) * fees
        ev = net / len(sel) if len(sel) else 0
        out.append({'th': th, 'n': len(sel), 'tp_rate': win, 'direct': direct,
                    'ev': ev})
    return pd.DataFrame(out)


def ev_series(features, feat, H, c, sl, side, fees=FEES_MAKER):
    """EV и точность по сетке порогов для одной конфигурации (feat,H,c,sl,side).
    TP-успех: atteглем цели раньше стопа; стоп-срабатывание отдельно не выводится."""

    px = features['price'].to_numpy()
    n = len(px)
    Hb = int(H)
    hit = np.zeros(n, dtype=bool)
    if side == 'long':
        for i in range(n - Hb):
            seg = px[i + 1:i + Hb + 1]
            hi, lo, p = seg.max(), seg.min(), px[i]
            hit[i] = (lo > p * (1 - sl)) and (hi >= p * (1 + c))
        col_direct = 'ret'  # вверх
    else:
        for i in range(n - Hb):
            seg = px[i + 1:i + Hb + 1]
            hi, lo, p = seg.max(), seg.min(), px[i]
            hit[i] = (hi < p * (1 + sl)) and (lo <= p * (1 - c))
        col_direct = 'ret'
    ret = features[f'ret_H{H}'].to_numpy()
    f = features[feat].to_numpy()
    lows = np.nanquantile(f, np.arange(0.0, 1.05, 0.05))
    rows = []
    for th in sorted(lows):
        sel = (f > th) if side == 'long' else (f < th)
        k = int(sel.sum())
        if k < 30:
            continue
        tw = int(hit[sel].sum())
        tp_rate = tw / k
        ev = (tp_rate * c - (1 - tp_rate) * sl) - fees
        direct = (ret[sel] > 0).mean() if side == 'long' else (ret[sel] < 0).mean()
        rows.append({'th': th, 'n': k, 'tp_rate': tp_rate, 'direct': direct, 'ev': ev})
    return pd.DataFrame(rows)


def evaluate_config(features, cfg):
    """EV выбранной конфигурации при фиксированном пороге из cfg (для test/дней)."""
    tb = ev_series(features, cfg['feat'], cfg['H'], cfg['c'], cfg['sl'], cfg['side'])
    if len(tb) == 0:
        return None
    i = np.nanargmin(np.abs(tb['th'].to_numpy() - cfg['th']))
    return tb.iloc[i]


def grid_search(features, horizons, targets, sls, fees=FEES_MAKER):
    """Grid по (feat, H, c, sl, side); для каждой ячейки порог — по лучшему EV."""
    rows = []
    feats = [c for c in features.columns if c.startswith('comb_W')]
    for feat in feats:
        for H in horizons:
            for c in targets:
                for sl in sls:
                    for side in ('long', 'short'):
                        tb = ev_series(features, feat, H, c, sl, side, fees)
                        if len(tb) == 0:
                            continue
                        b = tb.loc[tb['ev'].idxmax()]
                        rows.append({'feat': feat, 'H': H, 'c': c, 'sl': sl,
                                     'side': side, 'th': b['th'], 'n': b['n'],
                                     'tp_rate': b['tp_rate'], 'direct': b['direct'],
                                     'ev': b['ev']})
    return pd.DataFrame(rows)


def per_day_ev(features, cfg):
    """EV по дням для фиксированной конфигурации (проверка стабильности)."""
    out = []
    for dt, g in features.groupby(features['dt'].dt.date):
        r = evaluate_config(g, cfg)
        if r is None:
            continue
        out.append({'day': str(dt), 'n': r['n'], 'tp_rate': r['tp_rate'],
                    'direct': r['direct'], 'ev': r['ev']})
    return pd.DataFrame(out)


def base_rate(data, name, features, H, c):
    col_ret = f'ret_H{H}'
    col_hit = f'hit_H{H}_{int(c * 1e4)}'
    d = features[[col_ret, col_hit]].dropna()
    return {'direct_base': (d[col_ret] > 0).mean(),
            'tp_base': d[col_hit].mean()}


# ----------------------------------------------------------------------------
# HTML отчёт
# ----------------------------------------------------------------------------
def build_dashboard(data, name, features, corr, cons, evals, base,
                    targets, horizons_min, grid, best_cfg, te_ev, pdays):
    import plotly.graph_objects as go
    from plotly.subplots import make_subplots

    n = len(features)
    t0 = features['dt'].min()
    t1 = features['dt'].max()

    # 1) Цена + интегральный поток (дивергенция видна визуально)
    fig1 = go.Figure()
    fig1.add_trace(go.Scatter(x=features['dt'], y=features['price'], mode='lines',
                              name='Цена', line=dict(color='black', width=1.5)))
    fig1.add_trace(go.Scatter(x=features['dt'], y=features['intz_W180'],
                              mode='lines', name='intz W180 (з-поток)',
                              yaxis='y2', line=dict(color='steelblue', width=1.2)))
    fig1.update_layout(title='<b>Цена и интегрированный поток netFlow (W=3ч)</b>',
                       height=500,
                       xaxis_title='Время', yaxis_title='Цена',
                       yaxis2=dict(title='intz (z-шкала)', overlaying='y', side='right',
                                   showgrid=False),
                       hovermode='x unified', dragmode='pan',
                       xaxis=dict(rangeslider=dict(visible=True, thickness=0.05),
                                  rangeselector=dict(buttons=[
                                      dict(count=1, label='1д', step='day', stepmode='backward'),
                                      dict(step='all', label='Все')])))
    fig1.write_html(os.path.join(out_dir, 'ltfi_price.html'))

    # 2) Матрица корреляции feature x горизонт
    fig2 = go.Figure(go.Heatmap(
        z=corr.pivot_table(index='feature', columns='H_min', values='rho'),
        x=[f'{h} мин' for h in corr['H_min'].unique()],
        y=corr['feature'].unique(),
        colorscale='RdBu', zmid=0, zmin=-0.15, zmax=0.15,
        text=np.round(corr.pivot_table(index='feature', columns='H_min', values='rho').values, 3),
        texttemplate='%{text}', textfont=dict(size=9),
        hovertemplate='фича: %{y}<br>горизонт: %{x}<br>rho=%{z:.4f}<extra></extra>'))
    fig2.update_layout(title='<b>Корреляция СПирмена: фича (intz) vs форвард-движение</b>',
                       height=400, yaxis_autorange='reversed')
    fig2.write_html(os.path.join(out_dir, 'ltfi_correlation.html'))

    # 3) Стабильность по дням
    fig3 = go.Figure()
    for H in (180, 360):
        g = cons[cons['H_min'] == H]
        fig3.add_trace(go.Bar(x=g['day'], y=g['rho'], name=f'{H} мин',
                              hovertemplate='день: %{x}<br>rho=%{y:.3f}<br>p=%{customdata[0]:.3f}<extra></extra>',
                              customdata=g[['p']]))
    fig3.add_hline(y=0, line_color='gray', line_width=1)
    fig3.update_layout(title='<b>Корреляция intz W180 по дням (знак и стабильность)</b>',
                       height=400, barmode='group')
    fig3.write_html(os.path.join(out_dir, 'ltfi_consistency.html'))

    # 4) Кривые точности и EV по порогам (лучшие/выбранные комбы)
    fig4 = make_subplots(rows=1, cols=2, subplot_titles=('TP-доля и направление',
                                                          'EV сделки (maker)'))
    for label, tb in evals.items():
        if tb is None or len(tb) == 0:
            continue
        tb_l = tb[tb['side'] == 'long']
        tb_s = tb[tb['side'] == 'short']
        for name_suffix, tb_sel in ((' long', tb_l), (' short', tb_s)):
            if len(tb_sel) == 0:
                continue
            fig4.add_trace(go.Scatter(x=tb_sel['n'], y=tb_sel['tp_rate'], mode='lines+markers',
                                      name=f'TP-доля {label}{name_suffix}'), row=1, col=1)
            fig4.add_trace(go.Scatter(x=tb_sel['n'], y=tb_sel['direct'], mode='lines+markers',
                                      name=f'направл. {label}{name_suffix}',
                                      line=dict(dash='dot')), row=1, col=1)
            fig4.add_trace(go.Scatter(x=tb_sel['n'], y=tb_sel['ev'] * 1e4, mode='lines+markers',
                                      name=f'EV {label}{name_suffix}'), row=1, col=2)
    fig4.add_hline(y=0, line_color='gray', line_width=1, row=1, col=2)
    fig4.update_xaxes(title_text='число сигналов (по возрастанию жёсткости порога)', type='log')
    fig4.update_layout(title='<b>Эффективность сигнала по порогам (цель 0.5% + стоп 0.25%, H=6ч)</b>',
                       height=500, showlegend=True)
    fig4.write_html(os.path.join(out_dir, 'ltfi_curves.html'))

    # 5) HEATMAP EV по сетке TP x SL (лучший комбо-сигнал, на тесте)
    split = int(len(features) * TRAIN_RATIO)
    tr_f = features.iloc[:split]
    te_f = features.iloc[split:]
    feat, H, side = best_cfg['feat'], int(best_cfg['H']), best_cfg['side']
    zmat = np.full((len(SLS), len(TARGETS)), np.nan)
    for j, c in enumerate(TARGETS):
        for i, sl in enumerate(SLS):
            cell_cfg = dict(feat=feat, H=H, c=c, sl=sl, side=side, th=0.0)
            tb_tr = ev_series(tr_f, feat, H, c, sl, side)
            if len(tb_tr) == 0:
                continue
            cell_cfg['th'] = tb_tr.loc[tb_tr['ev'].idxmax(), 'th']
            r = evaluate_config(te_f, cell_cfg)
            if r is not None:
                zmat[i, j] = r['ev'] * 1e4
    fig5 = go.Figure(go.Heatmap(
        z=zmat, x=[f'TP {c:.2%}' for c in TARGETS], y=[f'SL {s:.2%}' for s in SLS],
        colorscale='RdYlGn', zmid=0,
        text=np.round(zmat, 1), texttemplate='%{text}', textfont=dict(size=10),
        hovertemplate='TP: %{x}<br>SL: %{y}<br>EV тест: %{z:.1f} бп<extra></extra>'))
    fig5.update_layout(
        title=f'<b>EV на TEST (бп/сделку) по сетке TP×SL — {feat} H={H} {side}</b>',
        height=420, xaxis_title='Цель', yaxis_title='Стоп')
    fig5.write_html(os.path.join(out_dir, 'ltfi_heatmap.html'))

    # 6) EV по дням на тесте для лучшего конфига
    fig6 = go.Figure()
    fig6.add_trace(go.Bar(x=pdays['day'], y=pdays['ev'] * 1e4,
                          marker_color=['#2e7d32' if v > 0 else '#c62828'
                                        for v in pdays['ev']],
                          hovertemplate='день: %{x}<br>EV=%{y:.1f} бп<br>'
                                        'n=%{customdata:.0f}<extra></extra>',
                          customdata=pdays['n']))
    fig6.add_hline(y=0, line_color='gray', line_width=1)
    fig6.update_layout(title='<b>EV по дням на TEST (лучший комбо-конфиг)</b>',
                       height=350, yaxis_title='EV, бп/сделку')
    fig6.write_html(os.path.join(out_dir, 'ltfi_days.html'))

    # 5) сводный HTML в стиле существующих дашбордов
    html = f'''<!DOCTYPE html>
<html>
<head><title>LTFI Анализ — сводка</title>
<style>
 body {{ font-family:'Segoe UI',Arial,sans-serif; background:#f0f2f5; margin:0; padding:20px; }}
 h1 {{ color:#1a1a2e; text-align:center; font-size:26px; }}
 .subtitle {{ text-align:center; color:#666; font-size:14px; margin-bottom:20px; }}
 .stats {{ background:white; border-radius:12px; padding:20px; margin:20px 0; box-shadow:0 4px 20px rgba(0,0,0,0.08); }}
 .stats-grid {{ display:grid; grid-template-columns:repeat(auto-fit,minmax(200px,1fr)); gap:12px; }}
 .stats-item {{ background:#f8f9fa; padding:10px 14px; border-radius:8px; border-left:4px solid #4a90d9; }}
 .stats-item .label {{ font-size:11px; color:#888; text-transform:uppercase; }}
 .stats-item .value {{ font-size:17px; font-weight:600; color:#1a1a2e; margin-top:2px; }}
 .stats-item .value.pos {{ color:#2e7d32; }} .stats-item .value.neg {{ color:#c62828; }}
 .graph {{ background:white; border-radius:12px; margin:20px 0; padding:16px; box-shadow:0 4px 20px rgba(0,0,0,0.08); }}
 .graph iframe {{ width:100%; border:none; border-radius:8px; }}
 table {{ border-collapse:collapse; width:100%; font-size:13px; }}
 td, th {{ border:1px solid #e0e0e0; padding:6px 10px; text-align:right; }}
 th {{ background:#f8f9fa; }}
</style></head>
<body>
<h1>📊 LTFI — длинный индикатор потока (часовой горизонт)</h1>
<p class="subtitle">Прокси накопленного потока; проверка корреляции с будущим движением на {min(horizons_min)}–{max(horizons_min)} мин</p>
<div class="stats"><div class="stats-grid">
 <div class="stats-item"><div class="label">Записей (лог)</div><div class="value">{n:,}</div></div>
 <div class="stats-item"><div class="label">Период</div><div class="value">{t0:%d.%m.%Y} – {t1:%d.%m.%Y}</div></div>
 <div class="stats-item"><div class="label">Баров (1 мин)</div><div class="value">{len(features):,}</div></div>
 <div class="stats-item"><div class="label">Base rate направл. (6ч)</div><div class="value">{base.get('direct_base', float('nan')):.1%}</div></div>
 <div class="stats-item"><div class="label">Base TP-доля (6ч)</div><div class="value">{base.get('tp_base', float('nan')):.1%}</div></div>
 {'<div class="stats-item"><div class="label">Лучший конфиг (EV тест)</div><div class="value">%.1f бп</div></div>' % (te_ev['ev']*1e4) if te_ev is not None else ''}
 {'<div class="stats-item"><div class="label">Лучший конфиг (фича/H)</div><div class="value">%s / %sч</div></div>' % (best_cfg['feat'], int(best_cfg['H'])//60) if best_cfg is not None else ''}
</div></div>
<div class="graph"><h3>📈 Цена + интегральный поток (W=3ч)</h3><iframe src="ltfi_price.html" height="520"></iframe></div>
<div class="graph"><h3>🌡 Корреляционная матрица intz (feature) × форвард-горизонт</h3><iframe src="ltfi_correlation.html" height="420"></iframe></div>
<div class="graph"><h3>📅 Стабильность корреляции по дням (это главный фильтр от ложных выводов)</h3><iframe src="ltfi_consistency.html" height="420"></iframe></div>
<div class="graph"><h3>💰 Кривые: TP-доля/направление/EV по порогам (H=6ч)</h3><iframe src="ltfi_curves.html" height="520"></iframe></div>
<div class="graph"><h3>🗺 EV на тесте по сетке TP × SL (комбо-сигнал)</h3><iframe src="ltfi_heatmap.html" height="440"></iframe></div>
<div class="graph"><h3>📅 EV по дням на тесте (лучший комбо-конфиг)</h3><iframe src="ltfi_days.html" height="370"></iframe></div>
<div class="footer" style="text-align:center;color:#999;font-size:12px;margin-top:30px">
 LTFI Analysis Tool v1.0 • данные: {TRADEFILE} (netFlow — нормированный поток, объёмный CVD не восстановим)</div>
</body></html>'''
    with open(os.path.join(out_dir, 'ltfi_dashboard.html'), 'w') as f:
        f.write(html)
    print('  HTML: ltfi_dashboard.html (и ltfi_*.html)')


# ----------------------------------------------------------------------------
# main
# ----------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--data', default=DEFAULT_DATA_DIR)
    ap.add_argument('--out', default=DEFAULT_OUT_DIR)
    ap.add_argument('--horizons', type=str, default='')
    args = ap.parse_args()

    global out_dir
    out_dir = args.out

    print('=' * 66)
    print('ДЛИННЫЙ TFI (LTFI) — ПРОВЕРКА НОВОГО ИНДИКАТОРА')
    print('=' * 66)

    print('\nЗагрузка логов ...')
    df = load_all(args.data)
    print(f'  Строк после дедупликации по ts: {len(df):,} '
          f'({df["ts"].min()} .. {df["ts"].max()})')

    bars = build_minute_bars(df)
    print(f'  Минутных баров: {len(bars):,}  '
          f'({bars["dt"].min()} .. {bars["dt"].max()})')

    horizons = tuple(int(x) for x in args.horizons.split(',')) if args.horizons \
        else HORIZONS_MIN

    print('\nРасчёт фич и меток ...')
    features = build_features(bars, WINDOWS_MIN)
    features = build_targets(features, horizons, TARGETS, SL)
    print(f'  Фич: ' + ', '.join(c for c in features.columns if not c.startswith(('dt', 'ts', 'price', 'ret', 'hit'))))

    print('\n' + '-' * 66)
    print('КОРРЕЛЯЦИЯ intz / tr / comb c форвард-движением (Spearman)')
    print('  tr = движение цены за окно (моментум); comb = дивергенция поток/цена')
    print('-' * 66)
    corr = analyze(features, '', features, TARGETS, horizons)
    print(corr.to_string(index=False))
    sig = corr[corr['p_value'] < 0.05]
    neg = corr[corr['rho'] < 0]
    print(f'\n  Всего пар: {len(corr)}; значимых (p<0.05): {len(sig)}; '
          f'отрицательных: {len(neg)} ({len(neg)/max(len(corr),1):.0%})')

    print('\n' + '-' * 66)
    print('СТАБИЛЬНОСТЬ КОРРЕЛЯЦИИ ПО ДНЯМ (intz_W180)')
    print('-' * 66)
    cons = per_day_consistency(features, '', features, horizons)
    print(cons.to_string(index=False))
    if len(cons):
        for H in (180, 360):
            g = cons[cons['H_min'] == H]
            if len(g):
                pos = (g['rho'] > 0).mean()
                print(f'  H={H} мин: среднее rho={g["rho"].mean():+.4f}, '
                      f'доля дней с одинаковым знаком: {max(pos, 1 - pos):.0%}')

    print('\n' + '-' * 66)
    print('БАЗОВЫЕ СТАВКИ (что даёт "всегда в рынке")')
    print('-' * 66)
    base = base_rate(features, '', features, 360, TARGETS[2])
    print(f'  6ч: доля up-move = {base["direct_base"]:.1%}; '
          f'достижение {TARGETS[2]:.2%} cтопом {SL:.2%} = {base["tp_base"]:.1%}')

    print('\n' + '-' * 66)
    print('ПОРОГОВЫЙ АНАЛИЗ (H=6ч, цель/стоп, комиссии)')
    print('-' * 66)
    evals = {}
    for feat, label in [('intz_W180', 'intz W3ч'), ('slope_W180', 'slope W3ч'),
                        ('div_W180', 'div W3ч'), ('comb_W180', 'comb W3ч'),
                        ('comb_W360', 'comb W6ч')]:
        results = []
        for side in ('long', 'short'):
            tb = threshold_eval(features, '', features, feat, 360, TARGETS[2], SL,
                                FEES_MAKER, side=side)
            if len(tb):
                tb = tb.copy()
                tb['side'] = side
                results.append(tb)
        if not results:
            continue
        tb = pd.concat(results, ignore_index=True)
        evals[label] = tb
        best = tb.loc[tb['ev'].idxmax()]
        print(f'  {label:10s} лучшая сторона={best["side"]:5s} '
              f'EV={best["ev"]*1e4:+.2f} бп/сделку при п.={best["n"]:6.0f}, '
              f'TP-доля={best["tp_rate"]:.1%} (base {base["tp_base"]:.1%}), '
              f'направл.={best["direct"]:.1%}')

    print('\n' + '-' * 66)
    print('GRID-SEARCH: комбо-сигнал, сетка TP x SL x порог (сплит по времени)')
    print('-' * 66)
    split = int(len(features) * TRAIN_RATIO)
    tr = features.iloc[:split]
    te = features.iloc[split:]
    grid = grid_search(tr, (180, 360), TARGETS, SLS)
    print(f'  train: {len(tr)} баров, test: {len(te)} баров '
          f'(доля train {TRAIN_RATIO:.0%})')
    grid = grid.sort_values('ev', ascending=False)
    top = grid.head(8)
    print('\n  Топ конфигураций по EV (выбор на TRAIN):')
    print('  ' + top[['feat', 'H', 'c', 'sl', 'side', 'n', 'tp_rate',
                      'direct', 'ev']].to_string(index=False))
    best_cfg = grid.iloc[0].to_dict()
    tr_ev = evaluate_config(tr, best_cfg)
    te_ev = evaluate_config(te, best_cfg)
    print(f'\n  Лучший конфиг: {best_cfg["feat"]} H={best_cfg["H"]} '
          f'TP={best_cfg["c"]:.2%} SL={best_cfg["sl"]:.2%} '
          f'сторона={best_cfg["side"]} порог={best_cfg["th"]:.3f}')
    if tr_ev is not None and te_ev is not None:
        print(f'  EV train = {tr_ev["ev"]*1e4:+.2f} бп '
              f'(n={tr_ev["n"]:.0f}), EV TEST = {te_ev["ev"]*1e4:+.2f} бп '
              f'(n={te_ev["n"]:.0f}), test TP-доля = {te_ev["tp_rate"]:.1%}')
    pdays = per_day_ev(te, best_cfg)
    if len(pdays):
        print('\n  EV по дням на TEST (стабильность):')
        print('  ' + pdays.to_string(index=False))
        pos = (pdays['ev'] > 0).sum()
        print(f'  Дней с EV>0: {pos}/{len(pdays)}; суммарный EV='
              f'{pdays["ev"].sum()*1e4:+.1f} бп')
    best_cfg_global = best_cfg

    print('\n' + '-' * 66)
    print('ГЕНЕРАЦИЯ HTML ОТЧЁТА')
    print('-' * 66)
    try:
        build_dashboard(features, '', features, corr, cons, evals, base,
                        TARGETS, horizons, grid, best_cfg, te_ev, pdays)
    except Exception as e:
        import traceback
        traceback.print_exc()


if __name__ == '__main__':
    main()