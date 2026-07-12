import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from scipy.stats import norm
import warnings
warnings.filterwarnings('ignore')

# Настройка стиля
plt.style.use('seaborn-v0_8-darkgrid')
sns.set_palette("Set2")

# ============================================
# 1. ЗАГРУЗКА ДАННЫХ
# ============================================
print("=" * 60)
print("АНАЛИЗ КОМБИНИРОВАННЫХ СИГНАЛОВ (OBI + TFI)")
print("=" * 60)

# Загрузка данных
df = pd.read_csv('../log_files/total_signals.csv', delimiter='\t')

# Проверяем наличие колонок
required_cols = ['ts', 'mid_price', 'signal_obi', 'signal_tfi', 'signal_combined']
missing_cols = [col for col in required_cols if col not in df.columns]
if missing_cols:
    print(f"ОШИБКА: Отсутствуют колонки: {missing_cols}")
    print(f"Доступные колонки: {df.columns.tolist()}")
    exit()

print(f"\nЗагружено записей: {len(df)}")
print(f"Период: {pd.to_datetime(df['ts'].min(), unit='ms')} - {pd.to_datetime(df['ts'].max(), unit='ms')}")
print(f"Длительность: {(df['ts'].max() - df['ts'].min()) / 1000 / 60:.1f} минут")

# Преобразуем время
df['ts_s'] = df['ts'] / 1000  # секунды
df['ts_s_rel'] = df['ts_s'] - df['ts_s'].iloc[0]  # относительное время

# ============================================
# 2. БАЗОВАЯ СТАТИСТИКА
# ============================================
print("\n" + "=" * 60)
print("БАЗОВАЯ СТАТИСТИКА")
print("=" * 60)

# Статистика сигналов
signals = df[df['signal_combined'] != 0]
buy_signals = df[df['signal_combined'] == 1]
sell_signals = df[df['signal_combined'] == 2]

print(f"\nВсего записей: {len(df)}")
print(f"BUY сигналов (combined): {len(buy_signals)} ({len(buy_signals)/len(df)*100:.3f}%)")
print(f"SELL сигналов (combined): {len(sell_signals)} ({len(sell_signals)/len(df)*100:.3f}%)")
print(f"NONE: {len(df[df['signal_combined'] == 0])} ({len(df[df['signal_combined'] == 0])/len(df)*100:.3f}%)")

# Сравнение с отдельными индикаторами
print(f"\n=== СРАВНЕНИЕ С ИНДИВИДУАЛЬНЫМИ ИНДИКАТОРАМИ ===")
print(f"OBI сигналов: {len(df[df['signal_obi'] != 0])} ({len(df[df['signal_obi'] != 0])/len(df)*100:.3f}%)")
print(f"TFI сигналов: {len(df[df['signal_tfi'] != 0])} ({len(df[df['signal_tfi'] != 0])/len(df)*100:.3f}%)")
print(f"Combined сигналов: {len(signals)} ({len(signals)/len(df)*100:.3f}%)")

# Анализ совпадений
obi_signals = df[df['signal_obi'] != 0].copy()
tfi_signals = df[df['signal_tfi'] != 0].copy()

# Создаем временные метки для сравнения
obi_signals['ts_group'] = obi_signals['ts'] // 1000  # группируем по секундам
tfi_signals['ts_group'] = tfi_signals['ts'] // 1000

# Совпадение по направлению в пределах 1 секунды
matched = 0
obi_total = len(obi_signals)
tfi_total = len(tfi_signals)

for idx, row in obi_signals.iterrows():
    # Ищем TFI сигнал в пределах ±1 секунда
    mask = (tfi_signals['ts'] >= row['ts'] - 1000) & (tfi_signals['ts'] <= row['ts'] + 1000)
    nearby_tfi = tfi_signals[mask]
    if len(nearby_tfi) > 0:
        # Проверяем совпадение направления
        if (nearby_tfi['signal_tfi'] == row['signal_obi']).any():
            matched += 1

print(f"\n=== АНАЛИЗ СОВПАДЕНИЙ ===")
print(f"Всего OBI сигналов: {obi_total}")
print(f"Всего TFI сигналов: {tfi_total}")
print(f"Совпало по направлению (в пределах 1с): {matched} ({matched/obi_total*100 if obi_total > 0 else 0:.1f}%)")

# ============================================
# 3. ФУНКЦИЯ АНАЛИЗА СИГНАЛОВ
# ============================================
def analyze_signals(df, signal_type, horizon_sec=3300, target_move=0.0020, max_drawdown=0.0040):
    """
    Анализ качества сигналов

    Параметры:
    - df: DataFrame с данными
    - signal_type: 1 (BUY) или 2 (SELL)
    - horizon_sec: горизонт удержания (сек)
    - target_move: целевое движение (0.0020 = 0.20%)
    - max_drawdown: максимальная просадка для стоп-лосса (0.0040 = 0.40%)

    Возвращает: словарь с метриками
    """
    signals = df[df['signal_combined'] == signal_type].copy()

    if len(signals) == 0:
        return {
            'good': 0, 'false': 0, 'stopped_out': 0,
            'accuracy': 0, 'win_ratio': 0,
            'total_signals': 0, 'valid_signals': 0,
            'avg_move': 0, 'median_move': 0, 'std_move': 0,
            'avg_profit': 0, 'avg_loss': 0, 'profit_factor': 0,
            'max_move': 0, 'min_move': 0,
            'moves': []
        }

    good = 0
    false = 0
    stopped_out = 0
    moves = []
    profits = []
    losses = []

    for idx, row in signals.iterrows():
        entry_price = row['mid_price']
        entry_time = row['ts']

        # Берем будущие данные (по timestamp, а не по индексу)
        future = df[(df['ts'] >= entry_time) &
                    (df['ts'] <= entry_time + horizon_sec * 1000)]

        if len(future) < 2:
            continue

        max_price = future['mid_price'].max()
        min_price = future['mid_price'].min()

        if signal_type == 1:  # BUY
            # Проверяем стоп-лосс
            dd = (entry_price - min_price) / entry_price
            if dd > max_drawdown:
                stopped_out += 1
                losses.append(-dd)
                continue

            move = (max_price - entry_price) / entry_price
            hit = move >= target_move

            if hit:
                good += 1
                profits.append(move)
            else:
                false += 1
                losses.append(-move if move < 0 else 0)

            moves.append(move)

        else:  # SELL
            dd = (max_price - entry_price) / entry_price
            if dd > max_drawdown:
                stopped_out += 1
                losses.append(-dd)
                continue

            move = (entry_price - min_price) / entry_price
            hit = move >= target_move

            if hit:
                good += 1
                profits.append(move)
            else:
                false += 1
                losses.append(-move if move < 0 else 0)

            moves.append(move)

    total_valid = good + false
    accuracy = good / total_valid if total_valid > 0 else 0
    win_ratio = good / len(signals) if len(signals) > 0 else 0

    avg_move = np.mean(moves) if moves else 0
    median_move = np.median(moves) if moves else 0
    std_move = np.std(moves) if moves else 0
    max_move = np.max(moves) if moves else 0
    min_move = np.min(moves) if moves else 0

    avg_profit = np.mean(profits) if profits else 0
    avg_loss = abs(np.mean(losses)) if losses else 0
    profit_factor = (sum(profits) / abs(sum(losses))) if sum(losses) != 0 else 0

    return {
        'good': good,
        'false': false,
        'stopped_out': stopped_out,
        'accuracy': accuracy,
        'win_ratio': win_ratio,
        'total_signals': len(signals),
        'valid_signals': total_valid,
        'avg_move': avg_move,
        'median_move': median_move,
        'std_move': std_move,
        'max_move': max_move,
        'min_move': min_move,
        'avg_profit': avg_profit,
        'avg_loss': avg_loss,
        'profit_factor': profit_factor,
        'moves': moves
    }

# ============================================
# 4. АНАЛИЗ СИГНАЛОВ
# ============================================
print("\n" + "=" * 60)
print("АНАЛИЗ СИГНАЛОВ")
print("=" * 60)

# Параметры для анализа (можно менять)
TARGET_MOVE = 0.0020  # 0.20%
MAX_DD = 0.0040       # 0.40%
HORIZON_SEC = 3300    # 55 минут

print(f"\nПараметры анализа:")
print(f"  Целевое движение: {TARGET_MOVE:.2%}")
print(f"  Max просадка: {MAX_DD:.2%}")
print(f"  Горизонт: {HORIZON_SEC//60} минут {HORIZON_SEC%60} секунд")

buy_res = analyze_signals(df, 1, HORIZON_SEC, TARGET_MOVE, MAX_DD)
sell_res = analyze_signals(df, 2, HORIZON_SEC, TARGET_MOVE, MAX_DD)

print(f"\n=== BUY СИГНАЛЫ ===")
print(f"Всего: {buy_res['total_signals']}")
print(f"  Успешных: {buy_res['good']} ({buy_res['accuracy']:.2%})")
print(f"  Неудачных: {buy_res['false']}")
print(f"  Стоп-лосс: {buy_res['stopped_out']}")
print(f"Среднее движение: {buy_res['avg_move']:.4%}")
print(f"Медианное движение: {buy_res['median_move']:.4%}")
print(f"Стандартное отклонение: {buy_res['std_move']:.4%}")
print(f"Макс движение: {buy_res['max_move']:.4%}")
print(f"Мин движение: {buy_res['min_move']:.4%}")
print(f"Средний профит: {buy_res['avg_profit']:.4%}")
print(f"Средний убыток: {buy_res['avg_loss']:.4%}")
print(f"Profit Factor: {buy_res['profit_factor']:.2f}")

print(f"\n=== SELL СИГНАЛЫ ===")
print(f"Всего: {sell_res['total_signals']}")
print(f"  Успешных: {sell_res['good']} ({sell_res['accuracy']:.2%})")
print(f"  Неудачных: {sell_res['false']}")
print(f"  Стоп-лосс: {sell_res['stopped_out']}")
print(f"Среднее движение: {sell_res['avg_move']:.4%}")
print(f"Медианное движение: {sell_res['median_move']:.4%}")
print(f"Стандартное отклонение: {sell_res['std_move']:.4%}")
print(f"Макс движение: {sell_res['max_move']:.4%}")
print(f"Мин движение: {sell_res['min_move']:.4%}")
print(f"Средний профит: {sell_res['avg_profit']:.4%}")
print(f"Средний убыток: {sell_res['avg_loss']:.4%}")
print(f"Profit Factor: {sell_res['profit_factor']:.2f}")

# Общая точность
total_signals = buy_res['total_signals'] + sell_res['total_signals']
total_good = buy_res['good'] + sell_res['good']
total_acc = total_good / total_signals if total_signals > 0 else 0
total_stopped = buy_res['stopped_out'] + sell_res['stopped_out']

print(f"\n=== ОБЩАЯ ТОЧНОСТЬ ===")
print(f"Всего сигналов: {total_signals}")
print(f"Успешных: {total_good} ({total_acc:.2%})")
print(f"Стоп-лосс: {total_stopped} ({total_stopped/total_signals*100 if total_signals > 0 else 0:.1f}%)")

# Сравнение с отдельными индикаторами
print(f"\n=== СРАВНЕНИЕ С ИНДИВИДУАЛЬНЫМИ ИНДИКАТОРАМИ ===")
print(f"OBI accuracy: ~30-35% (из предыдущего анализа)")
print(f"TFI accuracy: ~50% (из предыдущего анализа)")
print(f"Combined accuracy: {total_acc:.2%}")

# ============================================
# 5. АНАЛИЗ ДЛЯ РАЗНЫХ ГОРИЗОНТОВ
# ============================================
print("\n" + "=" * 60)
print("АНАЛИЗ ДЛЯ РАЗНЫХ ГОРИЗОНТОВ")
print("=" * 60)

horizons = [300, 600, 900, 1200, 1800, 2400, 3000, 3300, 3600, 4200, 4800, 5400, 6000]
targets = [0.0015, 0.0020, 0.0025]

print("\nAccuracy для разных горизонтов и целей:")
print("-" * 70)
print(f"{'Горизонт':>10} | {'Цель':>8} | {'BUY':>8} | {'SELL':>8} | {'Total':>8} | {'Сигналов':>10}")
print("-" * 70)

for h in horizons:
    for t in targets:
        buy_res_h = analyze_signals(df, 1, h, t, MAX_DD)
        sell_res_h = analyze_signals(df, 2, h, t, MAX_DD)
        total = buy_res_h['total_signals'] + sell_res_h['total_signals']
        total_acc = (buy_res_h['good'] + sell_res_h['good']) / total if total > 0 else 0
        print(f"{h:>10}s | {t:>7.2%} | {buy_res_h['accuracy']:>7.2%} | "
              f"{sell_res_h['accuracy']:>7.2%} | {total_acc:>7.2%} | {total:>10}")

# ============================================
# 6. ВИЗУАЛИЗАЦИЯ
# ============================================
print("\n" + "=" * 60)
print("ПОСТРОЕНИЕ ГРАФИКОВ")
print("=" * 60)

fig = plt.figure(figsize=(16, 14))

# 6.1 Цена с сигналами
ax1 = plt.subplot(3, 3, 1)
# Берем участок с сигналами
signal_indices = df[df['signal_combined'] != 0].index
if len(signal_indices) > 0:
    start_idx = max(0, signal_indices[0] - 100)
    end_idx = min(len(df), signal_indices[-1] + 100)
    plot_df = df.iloc[start_idx:end_idx]

    ax1.plot(plot_df['ts_s_rel'], plot_df['mid_price'], 'k-', alpha=0.7, linewidth=1)

    # Отмечаем сигналы
    buy_plot = plot_df[plot_df['signal_combined'] == 1]
    sell_plot = plot_df[plot_df['signal_combined'] == 2]

    ax1.scatter(buy_plot['ts_s_rel'], buy_plot['mid_price'],
                marker='^', s=100, c='green', label='BUY', zorder=5)
    ax1.scatter(sell_plot['ts_s_rel'], sell_plot['mid_price'],
                marker='v', s=100, c='red', label='SELL', zorder=5)

    ax1.set_title('Цена с комбинированными сигналами', fontsize=12)
    ax1.set_xlabel('Время (сек)')
    ax1.set_ylabel('Mid Price')
    ax1.legend(fontsize=8)
    ax1.grid(True, alpha=0.3)

# 6.2 Распределение движений после сигналов
ax2 = plt.subplot(3, 3, 2)
if buy_res['moves'] or sell_res['moves']:
    all_moves = buy_res['moves'] + sell_res['moves']
    ax2.hist(all_moves, bins=30, alpha=0.7, color='steelblue', edgecolor='black')
    ax2.axvline(TARGET_MOVE, color='g', linestyle='--', linewidth=2, label=f'цель {TARGET_MOVE:.2%}')
    ax2.axvline(0, color='r', linestyle='-', linewidth=1, alpha=0.5)
    ax2.set_title(f'Распределение движений (n={len(all_moves)})', fontsize=12)
    ax2.set_xlabel('Движение')
    ax2.set_ylabel('Частота')
    ax2.legend(fontsize=8)
    ax2.grid(True, alpha=0.3)

# 6.3 Распределение сигналов по времени
ax3 = plt.subplot(3, 3, 3)
if len(signals) > 0:
    signals['hour'] = pd.to_datetime(signals['ts'], unit='ms').dt.hour
    hourly_counts = signals.groupby(['hour', 'signal_combined']).size().unstack(fill_value=0)

    if 1 in hourly_counts.columns and 2 in hourly_counts.columns:
        width = 0.35
        x = np.arange(len(hourly_counts.index))
        ax3.bar(x - width/2, hourly_counts[1], width, label='BUY', color='green', alpha=0.7)
        ax3.bar(x + width/2, hourly_counts[2], width, label='SELL', color='red', alpha=0.7)
        ax3.set_xticks(x)
        ax3.set_xticklabels(hourly_counts.index)
        ax3.set_title('Сигналы по часам', fontsize=12)
        ax3.set_xlabel('Час')
        ax3.set_ylabel('Количество сигналов')
        ax3.legend(fontsize=8)
        ax3.grid(True, alpha=0.3)

# 6.4 Распределение OBI сигналов
ax4 = plt.subplot(3, 3, 4)
obi_buy = df[df['signal_obi'] == 1]
obi_sell = df[df['signal_obi'] == 2]
obi_none = df[df['signal_obi'] == 0]
ax4.bar(['BUY', 'SELL', 'NONE'],
        [len(obi_buy), len(obi_sell), len(obi_none)],
        color=['green', 'red', 'gray'], alpha=0.7)
ax4.set_title('Распределение OBI сигналов', fontsize=12)
ax4.set_ylabel('Количество')
ax4.grid(True, alpha=0.3)

# 6.5 Распределение TFI сигналов
ax5 = plt.subplot(3, 3, 5)
tfi_buy = df[df['signal_tfi'] == 1]
tfi_sell = df[df['signal_tfi'] == 2]
tfi_none = df[df['signal_tfi'] == 0]
ax5.bar(['BUY', 'SELL', 'NONE'],
        [len(tfi_buy), len(tfi_sell), len(tfi_none)],
        color=['green', 'red', 'gray'], alpha=0.7)
ax5.set_title('Распределение TFI сигналов', fontsize=12)
ax5.set_ylabel('Количество')
ax5.grid(True, alpha=0.3)

# 6.6 Сравнение accuracy для разных горизонтов (при целе 0.20%)
ax6 = plt.subplot(3, 3, 6)
buy_accs = []
sell_accs = []
total_accs = []
h_values = []

for h in horizons:
    buy_res_h = analyze_signals(df, 1, h, TARGET_MOVE, MAX_DD)
    sell_res_h = analyze_signals(df, 2, h, TARGET_MOVE, MAX_DD)
    total = buy_res_h['total_signals'] + sell_res_h['total_signals']
    total_acc = (buy_res_h['good'] + sell_res_h['good']) / total if total > 0 else 0

    if total >= 5:  # только если есть сигналы
        buy_accs.append(buy_res_h['accuracy'])
        sell_accs.append(sell_res_h['accuracy'])
        total_accs.append(total_acc)
        h_values.append(h//60)  # в минутах

if h_values:
    ax6.plot(h_values, buy_accs, 'o-', label='BUY', color='green', linewidth=2)
    ax6.plot(h_values, sell_accs, 's-', label='SELL', color='red', linewidth=2)
    ax6.plot(h_values, total_accs, 'd-', label='Total', color='blue', linewidth=2)
    ax6.axhline(0.5, color='gray', linestyle='--', alpha=0.5)
    ax6.set_title('Accuracy vs горизонт', fontsize=12)
    ax6.set_xlabel('Горизонт (минуты)')
    ax6.set_ylabel('Accuracy')
    ax6.legend(fontsize=8)
    ax6.grid(True, alpha=0.3)

# 6.7 Среднее движение vs горизонт
ax7 = plt.subplot(3, 3, 7)
avg_moves = []
h_values = []

for h in horizons:
    buy_res_h = analyze_signals(df, 1, h, TARGET_MOVE, MAX_DD)
    sell_res_h = analyze_signals(df, 2, h, TARGET_MOVE, MAX_DD)
    total = buy_res_h['total_signals'] + sell_res_h['total_signals']

    if total >= 5:
        avg_move = (buy_res_h['avg_move'] * buy_res_h['total_signals'] +
                   sell_res_h['avg_move'] * sell_res_h['total_signals']) / total
        avg_moves.append(avg_move)
        h_values.append(h//60)

if h_values:
    ax7.plot(h_values, avg_moves, 'o-', color='purple', linewidth=2)
    ax7.axhline(TARGET_MOVE, color='g', linestyle='--', label=f'цель {TARGET_MOVE:.2%}')
    ax7.set_title('Среднее движение vs горизонт', fontsize=12)
    ax7.set_xlabel('Горизонт (минуты)')
    ax7.set_ylabel('Среднее движение')
    ax7.legend(fontsize=8)
    ax7.grid(True, alpha=0.3)

# 6.8 Распределение длительности между сигналами
ax8 = plt.subplot(3, 3, 8)
if len(signals) > 1:
    time_diff = signals['ts'].diff().dropna() / 1000  # в секундах
    ax8.hist(time_diff[time_diff < 3600], bins=50, alpha=0.7, color='coral', edgecolor='black')
    ax8.set_title('Распределение времени между сигналами', fontsize=12)
    ax8.set_xlabel('Время между сигналами (сек)')
    ax8.set_ylabel('Частота')
    ax8.grid(True, alpha=0.3)
    ax8.axvline(time_diff.median(), color='red', linestyle='--',
                label=f'медиана = {time_diff.median():.0f}с')
    ax8.legend(fontsize=8)

# 6.9 Соотношение сигналов OBI и TFI на combined сигналах
ax9 = plt.subplot(3, 3, 9)
if len(signals) > 0:
    # Для каждого combined сигнала смотрим, откуда он пришел
    combined_buy = signals[signals['signal_combined'] == 1]
    combined_sell = signals[signals['signal_combined'] == 2]

    # Проверяем совпадение с OBI и TFI
    obi_match = ((signals['signal_obi'] == signals['signal_combined'])).sum()
    tfi_match = ((signals['signal_tfi'] == signals['signal_combined'])).sum()
    both_match = ((signals['signal_obi'] == signals['signal_combined']) &
                  (signals['signal_tfi'] == signals['signal_combined'])).sum()

    ax9.bar(['OBI совпадает', 'TFI совпадает', 'Оба совпадают'],
            [obi_match, tfi_match, both_match],
            color=['orange', 'skyblue', 'green'], alpha=0.7)
    ax9.set_title('Источники комбинированных сигналов', fontsize=12)
    ax9.set_ylabel('Количество сигналов')
    ax9.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig('combined_signals_analysis.png', dpi=150, bbox_inches='tight')
print("\nГрафик сохранен: combined_signals_analysis.png")
plt.show()

# ============================================
# 7. ДОПОЛНИТЕЛЬНЫЙ АНАЛИЗ: КАЧЕСТВО СИГНАЛОВ
# ============================================
print("\n" + "=" * 60)
print("ДОПОЛНИТЕЛЬНЫЙ АНАЛИЗ")
print("=" * 60)

# Анализ успешных vs неуспешных сигналов (при горизонте 55 минут)
if len(signals) > 0:
    # Добавляем колонку с результатом
    signals_copy = signals.copy()
    signals_copy['move'] = 0.0
    signals_copy['success'] = False

    for idx, row in signals_copy.iterrows():
        entry = row['mid_price']
        future = df[(df['ts'] >= row['ts']) & (df['ts'] <= row['ts'] + HORIZON_SEC * 1000)]
        if len(future) > 0:
            if row['signal_combined'] == 1:
                move = (future['mid_price'].max() - entry) / entry
                signals_copy.loc[idx, 'move'] = move
                signals_copy.loc[idx, 'success'] = move >= TARGET_MOVE
            else:
                move = (entry - future['mid_price'].min()) / entry
                signals_copy.loc[idx, 'move'] = move
                signals_copy.loc[idx, 'success'] = move >= TARGET_MOVE

    # Статистика по успешным и неуспешным
    successful = signals_copy[signals_copy['success']]
    failed = signals_copy[~signals_copy['success']]

    print(f"\nУспешные сигналы:")
    print(f"  Количество: {len(successful)}")
    if len(successful) > 0:
        print(f"  Среднее движение: {successful['move'].mean():.4%}")
        print(f"  Медианное движение: {successful['move'].median():.4%}")
        print(f"  Макс движение: {successful['move'].max():.4%}")

    print(f"\nНеуспешные сигналы:")
    print(f"  Количество: {len(failed)}")
    if len(failed) > 0:
        print(f"  Среднее движение: {failed['move'].mean():.4%}")
        print(f"  Медианное движение: {failed['move'].median():.4%}")
        print(f"  Мин движение: {failed['move'].min():.4%}")

# ============================================
# 8. АНАЛИЗ СИГНАЛОВ ПО ИСТОЧНИКАМ
# ============================================
print("\n" + "=" * 60)
print("АНАЛИЗ ПО ИСТОЧНИКАМ СИГНАЛОВ")
print("=" * 60)

# Определяем типы комбинированных сигналов
# 1 = только OBI совпал с combined
# 2 = только TFI совпал с combined
# 3 = оба совпали

signals['source'] = 0
mask_both = (signals['signal_obi'] == signals['signal_combined']) & (signals['signal_tfi'] == signals['signal_combined'])
mask_only_obi = (signals['signal_obi'] == signals['signal_combined']) & (signals['signal_tfi'] != signals['signal_combined'])
mask_only_tfi = (signals['signal_obi'] != signals['signal_combined']) & (signals['signal_tfi'] == signals['signal_combined'])

signals.loc[mask_both, 'source'] = 3
signals.loc[mask_only_obi, 'source'] = 1
signals.loc[mask_only_tfi, 'source'] = 2

print(f"\nИсточники сигналов:")
print(f"  Только OBI: {len(signals[signals['source']==1])}")
print(f"  Только TFI: {len(signals[signals['source']==2])}")
print(f"  Оба индикатора: {len(signals[signals['source']==3])}")

# Анализ качества по источникам
for source in [1, 2, 3]:
    source_name = {1: 'Только OBI', 2: 'Только TFI', 3: 'Оба'}[source]
    source_signals = signals[signals['source'] == source]
    if len(source_signals) > 0:
        # Оцениваем качество
        good = 0
        for idx, row in source_signals.iterrows():
            entry = row['mid_price']
            future = df[(df['ts'] >= row['ts']) & (df['ts'] <= row['ts'] + HORIZON_SEC * 1000)]
            if len(future) > 0:
                if row['signal_combined'] == 1:
                    move = (future['mid_price'].max() - entry) / entry
                else:
                    move = (entry - future['mid_price'].min()) / entry
                if move >= TARGET_MOVE:
                    good += 1
        acc = good / len(source_signals) if len(source_signals) > 0 else 0
        print(f"  {source_name}: {len(source_signals)} сигналов, accuracy={acc:.2%}")

print("\n" + "=" * 60)
print("АНАЛИЗ ЗАВЕРШЕН")
print("=" * 60)
