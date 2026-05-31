import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from scipy.stats import zscore
import warnings
warnings.filterwarnings('ignore')

# Загрузка
df = pd.read_csv('../log_files/imbalance_3105_large .csv', delimiter='\t')
df.columns = ['timestamp_ns', 'mid_price', 'DW_OBI', 'OBI_recent', 'OBI_prev', 'z_score', 'signal']

print(df.shape)
df.head()


# 2. Общая статистика по сигналам
# ======================================
# Преобразуем timestamp в секунды
df['timestamp_s'] = df['timestamp_ns'] / 1e9
df['timestamp_s'] -= df['timestamp_s'].iloc[0]  # релатив

# Статистика сигналов
buy_signals = df[df['signal'] == 'buy']
sell_signals = df[df['signal'] == 'sell']

print(f"Всего записей: {len(df)}")
print(f"BUY сигналов: {len(buy_signals)} ({len(buy_signals)/len(df)*100:.3f}%)")
print(f"SELL сигналов: {len(sell_signals)} ({len(sell_signals)/len(df)*100:.3f}%)")
print(f"Unknown: {len(df[df['signal']=='unknown'])}")

# ====================

# 3. Распределение z-score и DW_OBI
# ===========================
print("\n=== Строим графики распределений ===")

fig, axes = plt.subplots(2, 2, figsize=(12, 8))

# z-score
axes[0,0].hist(df['z_score'], bins=100, alpha=0.7, color='blue', edgecolor='black')
axes[0,0].set_title('Распределение z-score')
axes[0,0].set_xlabel('z-score')
axes[0,0].set_ylabel('Частота')
axes[0,0].axvline(3, color='r', linestyle='--', linewidth=2, label='threshold buy')
axes[0,0].axvline(-3, color='g', linestyle='--', linewidth=2, label='threshold sell')
axes[0,0].legend()

# z-score у сигналов
buy_signals = df[df['signal'] == 'buy']
sell_signals = df[df['signal'] == 'sell']

if len(buy_signals) > 0:
    axes[0,1].hist(buy_signals['z_score'], bins=20, alpha=0.7, color='green', label='buy', edgecolor='black')
if len(sell_signals) > 0:
    axes[0,1].hist(sell_signals['z_score'], bins=20, alpha=0.7, color='red', label='sell', edgecolor='black')
axes[0,1].set_title('z-score при сигналах')
axes[0,1].set_xlabel('z-score')
axes[0,1].set_ylabel('Частота')
axes[0,1].legend()

# DW_OBI
axes[1,0].hist(df['DW_OBI'], bins=100, alpha=0.7, color='purple', edgecolor='black')
axes[1,0].set_title('Распределение DW_OBI')
axes[1,0].set_xlabel('DW_OBI')
axes[1,0].set_ylabel('Частота')

# OBI_recent vs OBI_prev
axes[1,1].scatter(df['OBI_recent'], df['OBI_prev'], s=1, alpha=0.3)
axes[1,1].plot([-1,1],[-1,1], 'r--', linewidth=1)
axes[1,1].set_title('OBI_recent vs OBI_prev')
axes[1,1].set_xlabel('OBI_recent')
axes[1,1].set_ylabel('OBI_prev')

plt.tight_layout()

# Сохраняем график в файл (на всякий случай)
plt.savefig('distribution_analysis.png', dpi=150, bbox_inches='tight')
print("График сохранен в файл 'distribution_analysis.png'")

# Показываем график
plt.show()

print("\n=== Статистика ===")
print(f"Всего записей: {len(df)}")
print(f"BUY сигналов: {len(buy_signals)} ({len(buy_signals)/len(df)*100:.3f}%)")
print(f"SELL сигналов: {len(sell_signals)} ({len(sell_signals)/len(df)*100:.3f}%)")
print(f"Unknown: {len(df[df['signal']=='unknown'])}")

# Базовая статистика по z-score
print(f"\nСтатистика z-score:")
print(f"  Среднее: {df['z_score'].mean():.3f}")
print(f"  Стандартное отклонение: {df['z_score'].std():.3f}")
print(f"  Минимум: {df['z_score'].min():.3f}")
print(f"  Максимум: {df['z_score'].max():.3f}")
print(f"  >3: {(df['z_score'] > 3).sum()} записей ({(df['z_score'] > 3).sum()/len(df)*100:.3f}%)")
print(f"  <-3: {(df['z_score'] < -3).sum()} записей ({(df['z_score'] < -3).sum()/len(df)*100:.3f}%)")

# 4. Оценка качества сигналов (предсказание движения)
# ====================
def check_movement(df, signal_type, horizon_sec=120, target_move=0.0015):
    """
    horizon_sec: 120 секунд = 2 минуты
    target_move: 0.0015 = 0.15%
    """
    signals = df[df['signal'] == signal_type].copy()
    good = 0
    false = 0
    results = []

    for idx, row in signals.iterrows():
        entry_price = row['mid_price']
        entry_time = row['timestamp_s']
        future_rows = df[(df['timestamp_s'] >= entry_time) &
                         (df['timestamp_s'] <= entry_time + horizon_sec)]
        if len(future_rows) == 0:
            continue

        max_price = future_rows['mid_price'].max()
        min_price = future_rows['mid_price'].min()

        if signal_type == 'buy':
            move_pct = (max_price - entry_price) / entry_price
            hit = move_pct >= target_move
        else:
            move_pct = (entry_price - min_price) / entry_price
            hit = move_pct >= target_move

        if hit:
            good += 1
        else:
            false += 1
        results.append({'time': entry_time, 'price': entry_price,
                        'move_pct': move_pct, 'hit': hit})

    accuracy = good / (good + false) if (good+false) > 0 else 0
    return good, false, accuracy, pd.DataFrame(results)

buy_res = check_movement(df, 'buy', horizon_sec=120, target_move=0.0015)
sell_res = check_movement(df, 'sell', horizon_sec=120, target_move=0.0015)

print("=== BUY signals ===")
print(f"Good: {buy_res[0]}, False: {buy_res[1]}, Accuracy: {buy_res[2]:.2%}")
print(buy_res[3].head())
print("\n=== SELL signals ===")
print(f"Good: {sell_res[0]}, False: {sell_res[1]}, Accuracy: {sell_res[2]:.2%}")

# Проверим на разных горизонтах:
# ====================================
horizons = [60, 120, 180]
targets = [0.0015, 0.002, 0.0025]

for h in horizons:
    for t in targets:
        buy_acc = check_movement(df, 'buy', h, t)[2]
        sell_acc = check_movement(df, 'sell', h, t)[2]
        print(f"H={h}s, T={t:.2%}: Buy acc={buy_acc:.0%}, Sell acc={sell_acc:.0%}")


# ====================================


# 5. Визуализация сигналов на графике цены
# Выбираем участок с несколькими сигналами
signal_times = df[df['signal'] != 'unknown']['timestamp_s'].values
if len(signal_times) > 0:
    start = max(0, signal_times[0] - 500)
    end = min(df['timestamp_s'].max(), signal_times[-1] + 500)
else:
    start, end = 0, len(df)

mask = (df['timestamp_s'] >= start) & (df['timestamp_s'] <= end)
plot_df = df[mask]

plt.figure(figsize=(14, 8))
plt.plot(plot_df['timestamp_s'], plot_df['mid_price'], 'k-', alpha=0.7, linewidth=0.8, label='mid price')

# Отмечаем сигналы
buy_ts = plot_df[plot_df['signal'] == 'buy']['timestamp_s']
buy_price = plot_df[plot_df['signal'] == 'buy']['mid_price']
sell_ts = plot_df[plot_df['signal'] == 'sell']['timestamp_s']
sell_price = plot_df[plot_df['signal'] == 'sell']['mid_price']

plt.scatter(buy_ts, buy_price, marker='^', s=100, c='green', label='BUY signal', zorder=5)
plt.scatter(sell_ts, sell_price, marker='v', s=100, c='red', label='SELL signal', zorder=5)

plt.title('Цена BTCUSDT с сигналами индикатора')
plt.xlabel('Время (секунды от начала)')
plt.ylabel('Mid price')
plt.legend()
plt.grid(True, alpha=0.3)
plt.tight_layout()

# Сохраняем график в файл (на всякий случай)
plt.savefig('price.png', dpi=150, bbox_inches='tight')
print("График сохранен в файл 'price.png'")

# Показываем график
plt.show()


# 6. Анализ ложных/истинных на примере одного сигнала
#  ====================================
# Берем первый buy сигнал
first_buy = df[df['signal'] == 'buy'].iloc[0]
entry_idx = first_buy.name
look_ahead = 2000 # 2000 записей ~ 40 секунд? Зависит от частоты

end_idx = min(len(df), entry_idx + 2000)
segment = df.iloc[entry_idx:end_idx]

plt.figure(figsize=(12, 4))
plt.plot(segment['timestamp_s'] - segment['timestamp_s'].iloc[0],
         segment['mid_price'], 'b-', linewidth=1.5)
plt.axhline(y=first_buy['mid_price'], color='g', linestyle='--',
            label=f"entry {first_buy['mid_price']:.2f}")
plt.title(f'BUY signal at {first_buy["timestamp_s"]:.0f}s, z={first_buy["z_score"]:.2f}')
plt.xlabel('Seconds after signal')
plt.ylabel('Mid price')
plt.grid(True)
plt.legend()
plt.show()

# 7. Оптимизация порогов
# ====================================
# Перебираем разные пороги z-score и min shift
best = {'z': 3.0, 'shift': 0.05, 'acc': 0}
for z_thresh in np.arange(2.5, 4.0, 0.25):
    for shift_min in [0.03, 0.05, 0.07, 0.1]:
        # Эмулируем генерацию сигналов на истории
        df_temp = df.copy()
        shift = df_temp['OBI_recent'] - df_temp['OBI_prev']
        df_temp['signal_sim'] = 'unknown'
        df_temp.loc[(df_temp['z_score'] >= z_thresh) & (shift >= shift_min), 'signal_sim'] = 'buy'
        df_temp.loc[(df_temp['z_score'] <= -z_thresh) & (shift <= -shift_min), 'signal_sim'] = 'sell'

        buy_acc = check_movement(df_temp[df_temp['signal_sim']=='buy'], 'buy', 120, 0.0015)[2]
        sell_acc = check_movement(df_temp[df_temp['signal_sim']=='sell'], 'sell', 120, 0.0015)[2]
        avg_acc = (buy_acc + sell_acc) / 2

        if avg_acc > best['acc']:
            best = {'z': z_thresh, 'shift': shift_min, 'acc': avg_acc,
                    'buy_acc': buy_acc, 'sell_acc': sell_acc}

print(f"Optimal: z={best['z']}, shift={best['shift']}, avg_acc={best['acc']:.2%}")
