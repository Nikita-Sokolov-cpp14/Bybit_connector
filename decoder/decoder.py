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

# ==============================

# Посмотрим на z-score у сигналов
print("\nZ-score у BUY сигналов:")
print(buy_signals['z_score'].describe())
print("\nZ-score у SELL сигналов:")
print(sell_signals['z_score'].describe())

# Посмотрим на сдвиг OBI у сигналов
buy_signals['shift'] = buy_signals['OBI_recent'] - buy_signals['OBI_prev']
sell_signals['shift'] = sell_signals['OBI_recent'] - sell_signals['OBI_prev']
print(f"\nBUY shift: mean={buy_signals['shift'].mean():.4f}, min={buy_signals['shift'].min():.4f}")
print(f"SELL shift: mean={sell_signals['shift'].mean():.4f}, max={sell_signals['shift'].max():.4f}")

# Лаги от -60 до +60 тиков
for lag in range(-60, 61, 10):
    df[f'return_{lag}'] = df['mid_price'].shift(-lag) / df['mid_price'] - 1
    corr = df['z_score'].corr(df[f'return_{lag}'])
    if lag % 20 == 0:
        print(f"Lag {lag:3d}: correlation = {corr:.4f}")
print('\n')

# 4. Оценка качества сигналов (предсказание движения)
# ====================
def check_movement(df, signal_type, signal_column='signal',
                   horizon_sec=120, target_move=0.0015, max_drawdown=0.005):
    """
    Оценка сигналов
    signal_column: имя колонки с сигналами ('signal' или 'signal_sim')
    """
    # Используем указанную колонку для фильтрации
    signals = df[df[signal_column] == signal_type].copy()

    if len(signals) == 0:
        return {
            'good': 0, 'false': 0, 'stopped_out': 0,
            'accuracy': 0, 'total_signals': 0, 'valid_signals': 0
        }

    good = 0
    false = 0
    stopped_out = 0

    for idx, row in signals.iterrows():
        entry_price = row['mid_price']
        entry_time = row['timestamp_s']

        # Берем будущие данные из исходного df (НЕ ИЗ signals!)
        future_rows = df[(df['timestamp_s'] >= entry_time) &
                         (df['timestamp_s'] <= entry_time + horizon_sec)]

        if len(future_rows) == 0:
            continue

        max_price = future_rows['mid_price'].max()
        min_price = future_rows['mid_price'].min()

        if signal_type == 'buy':
            max_drawdown_actual = (entry_price - min_price) / entry_price
            if max_drawdown_actual > max_drawdown:
                stopped_out += 1
                continue
            move_pct = (max_price - entry_price) / entry_price
            hit = move_pct >= target_move
        else:
            max_drawdown_actual = (max_price - entry_price) / entry_price
            if max_drawdown_actual > max_drawdown:
                stopped_out += 1
                continue
            move_pct = (entry_price - min_price) / entry_price
            hit = move_pct >= target_move

        if hit:
            good += 1
        else:
            false += 1

    total_valid = good + false
    accuracy = good / total_valid if total_valid > 0 else 0

    return {
        'good': good, 'false': false, 'stopped_out': stopped_out,
        'accuracy': accuracy, 'total_signals': len(signals),
        'valid_signals': total_valid
    }

# Пример использования
result = check_movement(df, 'buy', signal_column='signal',
                        horizon_sec=120, target_move=0.0015, max_drawdown=0.005)
print(f"BUY signals analysis:")
print(f"  Total: {result['total_signals']}")
print(f"  Valid (no stop-out): {result['valid_signals']}")
print(f"  Stopped out: {result['stopped_out']}")
print(f"  Good: {result['good']}, False: {result['false']}")
print(f"  Accuracy: {result['accuracy']:.2%}")

# Проверим на разных горизонтах:
# ====================================
# horizons = [60, 120, 180]
# targets = [0.0015, 0.002, 0.0025]

# for h in horizons:
#     for t in targets:
#         buy_acc = check_movement(df, 'buy', h, t)[2]
#         sell_acc = check_movement(df, 'sell', h, t)[2]
#         print(f"H={h}s, T={t:.2%}: Buy acc={buy_acc:.0%}, Sell acc={sell_acc:.0%}")


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
# best = {'z': 3.0, 'shift': 0.05, 'acc': 0}
# for z_thresh in np.arange(2.5, 4.0, 0.25):
#     for shift_min in [0.03, 0.05, 0.07, 0.1]:
#         # Эмулируем генерацию сигналов на истории
#         df_temp = df.copy()
#         shift = df_temp['OBI_recent'] - df_temp['OBI_prev']
#         df_temp['signal_sim'] = 'unknown'
#         df_temp.loc[(df_temp['z_score'] >= z_thresh) & (shift >= shift_min), 'signal_sim'] = 'buy'
#         df_temp.loc[(df_temp['z_score'] <= -z_thresh) & (shift <= -shift_min), 'signal_sim'] = 'sell'

#         buy_acc = check_movement(df_temp[df_temp['signal_sim']=='buy'], 'buy', 120, 0.0015)[2]
#         sell_acc = check_movement(df_temp[df_temp['signal_sim']=='sell'], 'sell', 120, 0.0015)[2]
#         avg_acc = (buy_acc + sell_acc) / 2

#         if avg_acc > best['acc']:
#             best = {'z': z_thresh, 'shift': shift_min, 'acc': avg_acc,
#                     'buy_acc': buy_acc, 'sell_acc': sell_acc}

# print(f"Optimal: z={best['z']}, shift={best['shift']}, avg_acc={best['acc']:.2%}")

def optimize_with_split(df, train_ratio=0.7):
    """
    Правильная оптимизация: train/test split
    """
    # Разделяем данные
    split_idx = int(len(df) * train_ratio)
    train_df = df.iloc[:split_idx].copy()
    test_df = df.iloc[split_idx:].copy()

    best = None
    results = []

    print("Перебираем параметры...")

    for z_thresh in np.arange(2.5, 4.0, 0.25):
        for shift_min in [0.03, 0.05, 0.07, 0.1, 0.15]:
            # Генерируем сигналы на train
            train_df_temp = train_df.copy()
            shift = train_df_temp['OBI_recent'] - train_df_temp['OBI_prev']
            train_df_temp['signal_sim'] = 'unknown'
            train_df_temp.loc[(train_df_temp['z_score'] >= z_thresh) & (shift >= shift_min), 'signal_sim'] = 'buy'
            train_df_temp.loc[(train_df_temp['z_score'] <= -z_thresh) & (shift <= -shift_min), 'signal_sim'] = 'sell'

            # Оцениваем на train (используем signal_column='signal_sim')
            buy_result = check_movement(train_df_temp, 'buy', signal_column='signal_sim',
                                        horizon_sec=120, target_move=0.0015, max_drawdown=0.005)
            sell_result = check_movement(train_df_temp, 'sell', signal_column='signal_sim',
                                         horizon_sec=120, target_move=0.0015, max_drawdown=0.005)

            buy_acc = buy_result['accuracy']
            sell_acc = sell_result['accuracy']

            # Средняя точность (взвешенная по количеству сигналов)
            total_train_signals = buy_result['total_signals'] + sell_result['total_signals']
            if total_train_signals > 0:
                train_acc = (buy_acc * buy_result['total_signals'] + sell_acc * sell_result['total_signals']) / total_train_signals
            else:
                train_acc = 0

            # Генерируем сигналы на test
            test_df_temp = test_df.copy()
            shift = test_df_temp['OBI_recent'] - test_df_temp['OBI_prev']
            test_df_temp['signal_sim'] = 'unknown'
            test_df_temp.loc[(test_df_temp['z_score'] >= z_thresh) & (shift >= shift_min), 'signal_sim'] = 'buy'
            test_df_temp.loc[(test_df_temp['z_score'] <= -z_thresh) & (shift <= -shift_min), 'signal_sim'] = 'sell'

            buy_result_test = check_movement(test_df_temp, 'buy', signal_column='signal_sim',
                                             horizon_sec=120, target_move=0.0015, max_drawdown=0.005)
            sell_result_test = check_movement(test_df_temp, 'sell', signal_column='signal_sim',
                                              horizon_sec=120, target_move=0.0015, max_drawdown=0.005)

            buy_acc_test = buy_result_test['accuracy']
            sell_acc_test = sell_result_test['accuracy']

            total_test_signals = buy_result_test['total_signals'] + sell_result_test['total_signals']
            if total_test_signals > 0:
                test_acc = (buy_acc_test * buy_result_test['total_signals'] + sell_acc_test * sell_result_test['total_signals']) / total_test_signals
            else:
                test_acc = 0

            # Сохраняем результат, если есть сигналы
            if total_train_signals > 5 and total_test_signals > 5:
                result = {
                    'z': z_thresh,
                    'shift': shift_min,
                    'train_acc': train_acc,
                    'test_acc': test_acc,
                    'train_buy_signals': buy_result['total_signals'],
                    'train_sell_signals': sell_result['total_signals'],
                    'test_buy_signals': buy_result_test['total_signals'],
                    'test_sell_signals': sell_result_test['total_signals'],
                    'train_buy_good': buy_result['good'],
                    'train_sell_good': sell_result['good'],
                    'test_buy_good': buy_result_test['good'],
                    'test_sell_good': sell_result_test['good']
                }
                results.append(result)

                print(f"  z={z_thresh:.2f}, shift={shift_min:.2f}: "
                      f"train_acc={train_acc:.2%} ({total_train_signals} sig), "
                      f"test_acc={test_acc:.2%} ({total_test_signals} sig)")

    # Находим лучший по test_acc
    if results:
        results_df = pd.DataFrame(results)
        results_df = results_df.sort_values('test_acc', ascending=False)
        best = results_df.iloc[0].to_dict()

        print(f"\n=== Лучшие 5 комбинаций ===")
        print(results_df.head(5)[['z', 'shift', 'train_acc', 'test_acc',
                                   'train_buy_signals', 'train_sell_signals',
                                   'test_buy_signals', 'test_sell_signals']].to_string())
    else:
        print("Не найдено комбинаций с достаточным количеством сигналов!")
        best = {
            'z': 3.0, 'shift': 0.05, 'train_acc': 0, 'test_acc': 0,
            'train_buy_signals': 0, 'train_sell_signals': 0,
            'test_buy_signals': 0, 'test_sell_signals': 0,
            'train_buy_good': 0, 'train_sell_good': 0,
            'test_buy_good': 0, 'test_sell_good': 0
        }

    return best, results_df if results else pd.DataFrame()

# Запуск
print("\n=== Начинаем оптимизацию (может занять несколько минут) ===")
best_params = optimize_with_split(df)
print(f"\n=== РЕЗУЛЬТАТЫ ОПТИМИЗАЦИИ ===")
print(f"Best parameters:")
print(f"  z_threshold: {best_params['z']}")
print(f"  min_shift: {best_params['shift']}")
print(f"  Train accuracy: {best_params['train_acc']:.2%}")
print(f"  Test accuracy: {best_params['test_acc']:.2%}")
print(f"  Buy signals (train): {best_params['buy_signals_train']}")
print(f"  Sell signals (train): {best_params['sell_signals_train']}")
print(f"  Buy signals (test): {best_params['buy_signals_test']}")
print(f"  Sell signals (test): {best_params['sell_signals_test']}")
