import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from scipy.stats import norm, probplot
from scipy import stats
import warnings
warnings.filterwarnings('ignore')

# Настройка стиля
plt.style.use('seaborn-v0_8-darkgrid')
sns.set_palette("Set2")

# ============================================
# 1. ЗАГРУЗКА ДАННЫХ
# ============================================
print("=" * 60)
print("АНАЛИЗ TFI (Trade Flow Indicator)")
print("=" * 60)

# Загрузка
df = pd.read_csv('../log_files/trade_flow.csv', delimiter='\t')
df.columns = ['ts', 'mid_price', 'netFlow', 'mu', 'sigma', 'z_score',
              'signal', 'shortWin', 'baseWin', 'dataSize']

print(f"\nЗагружено записей: {len(df)}")
print(f"Период: {pd.to_datetime(df['ts'].min(), unit='ms')} - {pd.to_datetime(df['ts'].max(), unit='ms')}")
print(f"Длительность: {(df['ts'].max() - df['ts'].min()) / 1000 / 60:.1f} минут")

# Преобразуем время
df['ts_s'] = df['ts'] / 1000  # секунды
df['ts_s_rel'] = df['ts_s'] - df['ts_s'].iloc[0]  # относительное время

# Очистка: удаляем записи с недостаточными данными
df_clean = df[df['dataSize'] >= 20].copy()
print(f"После фильтрации (dataSize >= 20): {len(df_clean)} записей")

# ============================================
# 2. БАЗОВАЯ СТАТИСТИКА
# ============================================
print("\n" + "=" * 60)
print("БАЗОВАЯ СТАТИСТИКА")
print("=" * 60)

# Статистика сигналов
signals = df_clean[df_clean['signal'] != 0]
buy_signals = df_clean[df_clean['signal'] == 1]
sell_signals = df_clean[df_clean['signal'] == 2]

print(f"\nВсего записей: {len(df_clean)}")
print(f"BUY сигналов: {len(buy_signals)} ({len(buy_signals)/len(df_clean)*100:.3f}%)")
print(f"SELL сигналов: {len(sell_signals)} ({len(sell_signals)/len(df_clean)*100:.3f}%)")
print(f"NONE: {len(df_clean[df_clean['signal']== 0])} ({len(df_clean[df_clean['signal']== 0])/len(df_clean)*100:.3f}%)")

# Статистика z-score
print(f"\nСтатистика z-score:")
print(f"  Среднее: {df_clean['z_score'].mean():.3f}")
print(f"  Станд. отклонение: {df_clean['z_score'].std():.3f}")
print(f"  Минимум: {df_clean['z_score'].min():.3f}")
print(f"  Максимум: {df_clean['z_score'].max():.3f}")
print(f"  > 2.0: {(df_clean['z_score'] > 2.0).sum()} ({((df_clean['z_score'] > 2.0).sum()/len(df_clean)*100):.3f}%)")
print(f"  < -2.0: {(df_clean['z_score'] < -2.0).sum()} ({((df_clean['z_score'] < -2.0).sum()/len(df_clean)*100):.3f}%)")

# Статистика sigma
print(f"\nСтатистика sigma:")
print(f"  Среднее: {df_clean['sigma'].mean():.6f}")
print(f"  Медиана: {df_clean['sigma'].median():.6f}")
print(f"  Min: {df_clean['sigma'].min():.6f}")
print(f"  Max: {df_clean['sigma'].max():.6f}")

# Активность
print(f"\nАктивность торгов:")
print(f"  Среднее shortWin: {df_clean['shortWin'].mean():.1f}")
print(f"  Среднее baseWin: {df_clean['baseWin'].mean():.1f}")
print(f"  Среднее dataSize: {df_clean['dataSize'].mean():.1f} (из 20 интервалов)")

# ============================================
# 3. ФУНКЦИЯ АНАЛИЗА СИГНАЛОВ
# ============================================
# def analyze_signals(df, signal_type, horizon_sec=120, target_move=0.0020, max_drawdown=0.0040):
#     """
#     Анализ качества сигналов TFI

#     Параметры:
#     - df: DataFrame с данными
#     - signal_type: 1 или 2
#     - horizon_sec: горизонт удержания (сек)
#     - target_move: целевое движение (0.0020 = 0.20%)
#     - max_drawdown: максимальная просадка для стоп-лосса (0.0040 = 0.40%)

#     Возвращает: словарь с метриками
#     """
#     signals = df[df['signal'] == signal_type].copy()

#     if len(signals) == 0:
#         return {
#             'good': 0, 'false': 0, 'stopped_out': 0,
#             'accuracy': 0, 'total_signals': 0, 'valid_signals': 0,
#             'avg_move': 0, 'median_move': 0, 'win_ratio': 0,
#             'avg_profit': 0, 'avg_loss': 0, 'profit_factor': 0
#         }

#     good = 0
#     false = 0
#     stopped_out = 0
#     moves = []
#     profits = []
#     losses = []

#     for idx, row in signals.iterrows():
#         entry_price = row['mid_price']
#         entry_time = row['ts']

#         # Берем будущие данные
#         future = df[(df['ts'] >= entry_time) &
#                     (df['ts'] <= entry_time + horizon_sec * 1000)]

#         if len(future) < 2:
#             continue

#         max_price = future['mid_price'].max()
#         min_price = future['mid_price'].min()

#         if signal_type == 1:
#             # Проверяем стоп-лосс
#             dd = (entry_price - min_price) / entry_price
#             if dd > max_drawdown:
#                 stopped_out += 1
#                 losses.append(-dd)
#                 continue

#             move = (max_price - entry_price) / entry_price
#             hit = move >= target_move

#             if hit:
#                 good += 1
#                 profits.append(move)
#             else:
#                 false += 1
#                 losses.append(-move if move < 0 else 0)

#             moves.append(move)

#         else:  # SELL
#             dd = (max_price - entry_price) / entry_price
#             if dd > max_drawdown:
#                 stopped_out += 1
#                 losses.append(-dd)
#                 continue

#             move = (entry_price - min_price) / entry_price
#             hit = move >= target_move

#             if hit:
#                 good += 1
#                 profits.append(move)
#             else:
#                 false += 1
#                 losses.append(-move if move < 0 else 0)

#             moves.append(move)

#     total_valid = good + false
#     accuracy = good / total_valid if total_valid > 0 else 0
#     win_ratio = good / len(signals) if len(signals) > 0 else 0

#     avg_move = np.mean(moves) if moves else 0
#     median_move = np.median(moves) if moves else 0

#     avg_profit = np.mean(profits) if profits else 0
#     avg_loss = abs(np.mean(losses)) if losses else 0
#     profit_factor = (sum(profits) / abs(sum(losses))) if sum(losses) != 0 else 0

#     return {
#         'good': good,
#         'false': false,
#         'stopped_out': stopped_out,
#         'accuracy': accuracy,
#         'win_ratio': win_ratio,
#         'total_signals': len(signals),
#         'valid_signals': total_valid,
#         'avg_move': avg_move,
#         'median_move': median_move,
#         'avg_profit': avg_profit,
#         'avg_loss': avg_loss,
#         'profit_factor': profit_factor,
#         'moves': moves
#     }

# # ============================================
# # 4. АНАЛИЗ СИГНАЛОВ ПРИ ТЕКУЩИХ ПАРАМЕТРАХ
# # ============================================
# print("\n" + "=" * 60)
# print("АНАЛИЗ СИГНАЛОВ (z_thresh = 2.0)")
# print("=" * 60)

# TARGET_MOVE = 0.0020  # 0.20%
# MAX_DD = 0.0040       # 0.40%
# HORIZON = 3300         # 7 минут

# buy_res = analyze_signals(df_clean, 1, HORIZON, TARGET_MOVE, MAX_DD)
# sell_res = analyze_signals(df_clean, 2, HORIZON, TARGET_MOVE, MAX_DD)

# print(f"\n=== BUY СИГНАЛЫ ===")
# print(f"Всего: {buy_res['total_signals']}")
# print(f"  Успешных: {buy_res['good']} ({buy_res['accuracy']:.2%})")
# print(f"  Неудачных: {buy_res['false']}")
# print(f"  Стоп-лосс: {buy_res['stopped_out']}")
# print(f"Среднее движение: {buy_res['avg_move']:.4%}")
# print(f"Медианное движение: {buy_res['median_move']:.4%}")
# print(f"Средний профит: {buy_res['avg_profit']:.4%}")
# print(f"Средний убыток: {buy_res['avg_loss']:.4%}")
# print(f"Profit Factor: {buy_res['profit_factor']:.2f}")

# print(f"\n=== SELL СИГНАЛЫ ===")
# print(f"Всего: {sell_res['total_signals']}")
# print(f"  Успешных: {sell_res['good']} ({sell_res['accuracy']:.2%})")
# print(f"  Неудачных: {sell_res['false']}")
# print(f"  Стоп-лосс: {sell_res['stopped_out']}")
# print(f"Среднее движение: {sell_res['avg_move']:.4%}")
# print(f"Медианное движение: {sell_res['median_move']:.4%}")
# print(f"Средний профит: {sell_res['avg_profit']:.4%}")
# print(f"Средний убыток: {sell_res['avg_loss']:.4%}")
# print(f"Profit Factor: {sell_res['profit_factor']:.2f}")

# # Общая точность
# total_signals = buy_res['total_signals'] + sell_res['total_signals']
# total_good = buy_res['good'] + sell_res['good']
# total_acc = total_good / total_signals if total_signals > 0 else 0
# print(f"\n=== ОБЩАЯ ТОЧНОСТЬ ===")
# print(f"Всего сигналов: {total_signals}")
# print(f"Успешных: {total_good} ({total_acc:.2%})")

# # ============================================
# # 5. ОПТИМИЗАЦИЯ ПОРОГА Z-SCORE
# # ============================================
# print("\n" + "=" * 60)
# print("ОПТИМИЗАЦИЯ ПОРОГА Z-SCORE")
# print("=" * 60)

# def optimize_z_threshold(df, train_ratio=0.7, target_move=0.0020,
#                          max_drawdown=0.0040, horizon_sec=120):
#     """
#     Оптимизация порога z-score с разделением на train/test
#     """
#     # Разделяем данные
#     split_idx = int(len(df) * train_ratio)
#     train_df = df.iloc[:split_idx].copy()
#     test_df = df.iloc[split_idx:].copy()

#     results = []

#     print(f"\nTrain период: {len(train_df)} записей")
#     print(f"Test период: {len(test_df)} записей")
#     print("\nПеребор порогов z-score...")
#     print("-" * 60)

#     # Перебираем пороги от 1.0 до 4.0 с шагом 0.1
#     for z_thresh in np.arange(1.0, 4.1, 0.1):
#         # Генерируем сигналы на train
#         train_buy = train_df[train_df['z_score'] >= z_thresh]
#         train_sell = train_df[train_df['z_score'] <= -z_thresh]

#         # Оцениваем на train
#         buy_res = analyze_signals(train_buy, 1, horizon_sec, target_move, max_drawdown)
#         sell_res = analyze_signals(train_sell, 2, horizon_sec, target_move, max_drawdown)

#         train_total = buy_res['total_signals'] + sell_res['total_signals']
#         train_good = buy_res['good'] + sell_res['good']
#         train_acc = train_good / train_total if train_total > 0 else 0

#         # Оцениваем на test
#         test_buy = test_df[test_df['z_score'] >= z_thresh]
#         test_sell = test_df[test_df['z_score'] <= -z_thresh]

#         buy_res_test = analyze_signals(test_buy, 1, horizon_sec, target_move, max_drawdown)
#         sell_res_test = analyze_signals(test_sell, 2, horizon_sec, target_move, max_drawdown)

#         test_total = buy_res_test['total_signals'] + sell_res_test['total_signals']
#         test_good = buy_res_test['good'] + sell_res_test['good']
#         test_acc = test_good / test_total if test_total > 0 else 0

#         # Сохраняем только если есть сигналы
#         if train_total >= 5 and test_total >= 5:
#             results.append({
#                 'z_thresh': z_thresh,
#                 'train_acc': train_acc,
#                 'test_acc': test_acc,
#                 'train_total': train_total,
#                 'test_total': test_total,
#                 'train_good': train_good,
#                 'test_good': test_good,
#                 'train_buy': buy_res['total_signals'],
#                 'train_sell': sell_res['total_signals'],
#                 'test_buy': buy_res_test['total_signals'],
#                 'test_sell': sell_res_test['total_signals'],
#                 'train_buy_acc': buy_res['accuracy'],
#                 'train_sell_acc': sell_res['accuracy'],
#                 'test_buy_acc': buy_res_test['accuracy'],
#                 'test_sell_acc': sell_res_test['accuracy']
#             })

#             print(f"z={z_thresh:.1f}: train_acc={train_acc:.2%} ({train_total} sig), "
#                   f"test_acc={test_acc:.2%} ({test_total} sig)")

#     if not results:
#         print("Не найдено комбинаций с достаточным количеством сигналов!")
#         return None, None

#     # Находим лучший по test_acc
#     results_df = pd.DataFrame(results)
#     best = results_df.loc[results_df['test_acc'].idxmax()]

#     return best, results_df

# # Запуск оптимизации
# best_params, opt_results = optimize_z_threshold(
#     df_clean,
#     train_ratio=0.7,
#     target_move=TARGET_MOVE,
#     max_drawdown=MAX_DD,
#     horizon_sec=HORIZON
# )

# if best_params is not None:
#     print("\n" + "=" * 60)
#     print("РЕЗУЛЬТАТЫ ОПТИМИЗАЦИИ")
#     print("=" * 60)
#     print(f"\nОптимальный порог z-score: {best_params['z_thresh']:.1f}")
#     print(f"\nTrain accuracy: {best_params['train_acc']:.2%} ({best_params['train_total']} сигналов)")
#     print(f"  BUY: {best_params['train_buy']} сигналов, accuracy={best_params['train_buy_acc']:.2%}")
#     print(f"  SELL: {best_params['train_sell']} сигналов, accuracy={best_params['train_sell_acc']:.2%}")
#     print(f"\nTest accuracy: {best_params['test_acc']:.2%} ({best_params['test_total']} сигналов)")
#     print(f"  BUY: {best_params['test_buy']} сигналов, accuracy={best_params['test_buy_acc']:.2%}")
#     print(f"  SELL: {best_params['test_sell']} сигналов, accuracy={best_params['test_sell_acc']:.2%}")

#     # Топ-5 комбинаций
#     print("\nТоп-5 комбинаций:")
#     print(opt_results.head(5)[['z_thresh', 'train_acc', 'test_acc',
#                                 'train_total', 'test_total']].to_string(index=False))
# else:
#     print("\nОптимизация не удалась!")

# ============================================
# 6. ВИЗУАЛИЗАЦИЯ (СТАТИЧЕСКИЕ ГРАФИКИ)
# ============================================
# print("\n" + "=" * 60)
# print("ПОСТРОЕНИЕ СТАТИЧЕСКИХ ГРАФИКОВ")
# print("=" * 60)

# fig = plt.figure(figsize=(16, 14))

# # 6.1 Распределение z-score
# ax1 = plt.subplot(3, 3, 1)
# ax1.hist(df_clean['z_score'], bins=80, alpha=0.7, color='steelblue', edgecolor='black', density=True)
# # Нормальное распределение
# x = np.linspace(df_clean['z_score'].min(), df_clean['z_score'].max(), 100)
# ax1.plot(x, norm.pdf(x, 0, 1), 'r-', linewidth=2, label='N(0,1)')
# ax1.axvline(2.0, color='g', linestyle='--', linewidth=2, label='threshold ±2.0')
# ax1.axvline(-2.0, color='g', linestyle='--', linewidth=2)
# if best_params is not None:
#     ax1.axvline(best_params['z_thresh'], color='orange', linestyle=':', linewidth=2,
#                 label=f'optimal {best_params["z_thresh"]:.1f}')
#     ax1.axvline(-best_params['z_thresh'], color='orange', linestyle=':', linewidth=2)
# ax1.set_title('Распределение z-score', fontsize=12)
# ax1.set_xlabel('z-score')
# ax1.set_ylabel('Плотность')
# ax1.legend(fontsize=8)
# ax1.grid(True, alpha=0.3)

# # 6.2 QQ-plot z-score
# ax2 = plt.subplot(3, 3, 2)
# stats.probplot(df_clean['z_score'][df_clean['dataSize'] >= 20], dist="norm", plot=ax2)
# ax2.set_title('QQ-plot z-score', fontsize=12)
# ax2.grid(True, alpha=0.3)

# # 6.3 Распределение sigma
# ax3 = plt.subplot(3, 3, 3)
# ax3.hist(df_clean['sigma'], bins=50, alpha=0.7, color='coral', edgecolor='black')
# ax3.axvline(df_clean['sigma'].median(), color='red', linestyle='--',
#             label=f'медиана = {df_clean["sigma"].median():.6f}')
# ax3.set_title('Распределение sigma', fontsize=12)
# ax3.set_xlabel('sigma')
# ax3.set_ylabel('Частота')
# ax3.legend(fontsize=8)
# ax3.grid(True, alpha=0.3)

# # 6.4 Зависимость z-score от shortWin
# ax4 = plt.subplot(3, 3, 4)
# scatter = ax4.scatter(df_clean['shortWin'], df_clean['z_score'],
#                       c=df_clean['sigma'], cmap='viridis', alpha=0.5, s=3)
# ax4.axhline(2.0, color='g', linestyle='--', linewidth=1)
# ax4.axhline(-2.0, color='g', linestyle='--', linewidth=1)
# ax4.set_title('z-score vs shortWin (цвет = sigma)', fontsize=12)
# ax4.set_xlabel('Количество сделок в 300 мс (shortWin)')
# ax4.set_ylabel('z-score')
# plt.colorbar(scatter, ax=ax4, label='sigma')

# # 6.5 Зависимость accuracy от shortWin (для сигналов)
# ax5 = plt.subplot(3, 3, 5)
# # Группируем сигналы по shortWin
# signals_df = df_clean[df_clean['signal'] != 0].copy()
# if len(signals_df) > 0:
#     bins = np.arange(0, signals_df['shortWin'].max() + 5, 5)
#     signals_df['shortWin_bin'] = pd.cut(signals_df['shortWin'], bins=bins)

#     # Считаем accuracy для каждой группы
#     acc_by_win = []
#     for name, group in signals_df.groupby('shortWin_bin'):
#         if len(group) >= 10:
#             buy_acc = analyze_signals(group[group['signal'] == 1], 1,
#                                       HORIZON, TARGET_MOVE, MAX_DD)['accuracy']
#             sell_acc = analyze_signals(group[group['signal'] == 2], 2,
#                                        HORIZON, TARGET_MOVE, MAX_DD)['accuracy']
#             acc_by_win.append({
#                 'bin': name,
#                 'mid': (name.left + name.right) / 2,
#                 'accuracy': (buy_acc + sell_acc) / 2,
#                 'count': len(group)
#             })

#     if acc_by_win:
#         acc_df = pd.DataFrame(acc_by_win)
#         ax5.plot(acc_df['mid'], acc_df['accuracy'], 'o-', color='steelblue', linewidth=2)
#         ax5.axhline(0.5, color='red', linestyle='--', label='random')
#         ax5.set_title('Точность vs shortWin', fontsize=12)
#         ax5.set_xlabel('Количество сделок в 300 мс')
#         ax5.set_ylabel('Accuracy')
#         ax5.grid(True, alpha=0.3)
#         ax5.legend()

# # 6.6 Зависимость точности от sigma
# ax6 = plt.subplot(3, 3, 6)
# if len(signals_df) > 0:
#     bins = np.linspace(signals_df['sigma'].min(), signals_df['sigma'].max(), 20)
#     signals_df['sigma_bin'] = pd.cut(signals_df['sigma'], bins=bins)

#     acc_by_sigma = []
#     for name, group in signals_df.groupby('sigma_bin'):
#         if len(group) >= 10:
#             buy_acc = analyze_signals(group[group['signal'] == 1], 1,
#                                       HORIZON, TARGET_MOVE, MAX_DD)['accuracy']
#             sell_acc = analyze_signals(group[group['signal'] == 2], 2,
#                                        HORIZON, TARGET_MOVE, MAX_DD)['accuracy']
#             acc_by_sigma.append({
#                 'bin': name,
#                 'mid': (name.left + name.right) / 2,
#                 'accuracy': (buy_acc + sell_acc) / 2,
#                 'count': len(group)
#             })

#     if acc_by_sigma:
#         acc_df = pd.DataFrame(acc_by_sigma)
#         ax6.plot(acc_df['mid'], acc_df['accuracy'], 'o-', color='coral', linewidth=2)
#         ax6.axhline(0.5, color='red', linestyle='--', label='random')
#         ax6.set_title('Точность vs sigma', fontsize=12)
#         ax6.set_xlabel('sigma')
#         ax6.set_ylabel('Accuracy')
#         ax6.grid(True, alpha=0.3)
#         ax6.legend()

# # 6.7 Цена с сигналами (выборка)
# ax7 = plt.subplot(3, 3, 7)
# # Берем участок с сигналами
# signal_indices = df_clean[df_clean['signal'] != 0].index
# if len(signal_indices) > 0:
#     start_idx = max(0, signal_indices[0] - 100)
#     end_idx = min(len(df_clean), signal_indices[-1] + 100)
#     plot_df = df_clean.iloc[start_idx:end_idx]

#     ax7.plot(plot_df['ts_s_rel'], plot_df['mid_price'], 'k-', alpha=0.7, linewidth=1)

#     # Отмечаем сигналы
#     buy_plot = plot_df[plot_df['signal'] == 1]
#     sell_plot = plot_df[plot_df['signal'] == 2]

#     ax7.scatter(buy_plot['ts_s_rel'], buy_plot['mid_price'],
#                 marker='^', s=80, c='green', label='BUY', zorder=5)
#     ax7.scatter(sell_plot['ts_s_rel'], sell_plot['mid_price'],
#                 marker='v', s=80, c='red', label='SELL', zorder=5)

#     ax7.set_title('Цена с сигналами TFI', fontsize=12)
#     ax7.set_xlabel('Время (сек)')
#     ax7.set_ylabel('Mid Price')
#     ax7.legend(fontsize=8)
#     ax7.grid(True, alpha=0.3)

# # 6.8 Распределение сигналов по времени
# ax8 = plt.subplot(3, 3, 8)
# if len(signals_df) > 0:
#     signals_df['hour'] = pd.to_datetime(signals_df['ts'], unit='ms').dt.hour

#     # Считаем сигналы по часам
#     hourly_counts = signals_df.groupby(['hour', 'signal']).size().unstack(fill_value=0)

#     if 1 in hourly_counts.columns and 2 in hourly_counts.columns:
#         width = 0.35
#         x = np.arange(len(hourly_counts.index))
#         ax8.bar(x - width/2, hourly_counts[1], width, label='BUY', color='green', alpha=0.7)
#         ax8.bar(x + width/2, hourly_counts[2], width, label='SELL', color='red', alpha=0.7)
#         ax8.set_xticks(x)
#         ax8.set_xticklabels(hourly_counts.index)
#         ax8.set_title('Сигналы по часам', fontsize=12)
#         ax8.set_xlabel('Час (UTC)')
#         ax8.set_ylabel('Количество сигналов')
#         ax8.legend(fontsize=8)
#         ax8.grid(True, alpha=0.3)

# # 6.9 Результаты оптимизации
# ax9 = plt.subplot(3, 3, 9)
# if opt_results is not None and len(opt_results) > 0:
#     ax9.plot(opt_results['z_thresh'], opt_results['train_acc'], 'o-',
#              label='Train', color='blue', linewidth=2)
#     ax9.plot(opt_results['z_thresh'], opt_results['test_acc'], 's-',
#              label='Test', color='red', linewidth=2)
#     if best_params is not None:
#         ax9.axvline(best_params['z_thresh'], color='green', linestyle='--',
#                     label=f'best = {best_params["z_thresh"]:.1f}')
#     ax9.set_title('Оптимизация порога z-score', fontsize=12)
#     ax9.set_xlabel('Порог z-score')
#     ax9.set_ylabel('Accuracy')
#     ax9.legend(fontsize=8)
#     ax9.grid(True, alpha=0.3)

# plt.tight_layout()
# plt.savefig('tfi_analysis.png', dpi=150, bbox_inches='tight')
# print("\nГрафик сохранен: tfi_analysis.png")
# plt.show()

# # ============================================
# # 7. ДОПОЛНИТЕЛЬНЫЙ АНАЛИЗ: РАЗНЫЕ ГОРИЗОНТЫ
# # ============================================
# print("\n" + "=" * 60)
# print("АНАЛИЗ ДЛЯ РАЗНЫХ ГОРИЗОНТОВ")
# print("=" * 60)

# horizons = [2000, 3300, 4000]
# targets = [0.0015, 0.0020, 0.0025]

# print("\nAccuracy для разных горизонтов и целей:")
# print("-" * 60)
# print(f"{'Горизонт':>10} | {'Цель':>8} | {'BUY':>8} | {'SELL':>8} | {'Total':>8}")
# print("-" * 60)

# for h in horizons:
#     for t in targets:
#         buy_res_h = analyze_signals(df_clean[df_clean['signal'] == 1], 1, h, t, MAX_DD)
#         sell_res_h = analyze_signals(df_clean[df_clean['signal'] == 2], 2, h, t, MAX_DD)
#         total_acc = (buy_res_h['good'] + sell_res_h['good']) / \
#                     (buy_res_h['total_signals'] + sell_res_h['total_signals']) \
#                     if (buy_res_h['total_signals'] + sell_res_h['total_signals']) > 0 else 0
#         print(f"{h:>10}s | {t:>7.2%} | {buy_res_h['accuracy']:>7.2%} | "
#               f"{sell_res_h['accuracy']:>7.2%} | {total_acc:>7.2%}")

# ============================================
# 8. АНАЛИЗ ЛУЧШИХ/ХУДШИХ СИГНАЛОВ
# ============================================
# print("\n" + "=" * 60)
# print("АНАЛИЗ КАЧЕСТВА СИГНАЛОВ")
# print("=" * 60)

# # Смотрим на распределение движений после сигналов
# if len(signals_df) > 0:
#     # Получаем движения для всех сигналов
#     all_moves = []
#     for signal_type in [1, 2]:
#         sig_df = signals_df[signals_df['signal'] == signal_type]
#         for idx, row in sig_df.iterrows():
#             entry = row['mid_price']
#             future = df_clean[(df_clean['ts'] >= row['ts']) &
#                               (df_clean['ts'] <= row['ts'] + HORIZON * 1000)]
#             if len(future) > 0:
#                 if signal_type == 1:
#                     move = (future['mid_price'].max() - entry) / entry
#                 else:
#                     move = (entry - future['mid_price'].min()) / entry
#                 all_moves.append(move)

#     if all_moves:
#         print(f"\nРаспределение движений после сигналов:")
#         print(f"  Среднее: {np.mean(all_moves):.4%}")
#         print(f"  Медиана: {np.median(all_moves):.4%}")
#         print(f"  Стандартное отклонение: {np.std(all_moves):.4%}")
#         print(f"  Min: {np.min(all_moves):.4%}")
#         print(f"  Max: {np.max(all_moves):.4%}")
#         print(f"  Доля >= 0.20%: {np.mean(np.array(all_moves) >= TARGET_MOVE):.2%}")

# ============================================
# 9. ИНТЕРАКТИВНЫЕ HTML ГРАФИКИ (С СЭМПЛИРОВАНИЕМ)
# ============================================
print("\n" + "=" * 60)
print("ПОСТРОЕНИЕ ИНТЕРАКТИВНЫХ HTML ГРАФИКОВ")
print("=" * 60)

try:
    import plotly.graph_objects as go
    from plotly.subplots import make_subplots
    USE_PLOTLY = True
    print("✅ Используем Plotly для интерактивных графиков")
except ImportError:
    USE_PLOTLY = False
    print("⚠️ Plotly не установлен. Установите: pip install plotly")

if USE_PLOTLY:

    # Параметры сэмплирования
    SAMPLE_STEP = 1  # Берем каждую 2-ю точку
    print(f"\n📊 Сэмплирование данных: берем каждую {SAMPLE_STEP}-ю точку")
    print(f"   Исходное количество точек: {len(df_clean):,}")

    # Создаем сэмплированные данные для графиков
    df_sampled = df_clean.iloc[::SAMPLE_STEP].copy()
    print(f"   После сэмплирования: {len(df_sampled):,} точек")

    # Автоматический подбор порога (95-й перцентиль)
    recommended_threshold = np.percentile(np.abs(df_clean['z_score']), 95)

    # --------------------------------------------------------------------
    # 9.1 ИНТЕРАКТИВНЫЙ ГРАФИК: Цена с сигналами
    # --------------------------------------------------------------------
    print("\n9.1 Создание интерактивного графика: Цена с сигналами...")

    fig_price = go.Figure()

    fig_price.add_trace(
        go.Scatter(
            x=df_sampled['ts_s_rel'],
            y=df_sampled['mid_price'],
            mode='lines',
            name='Price',
            line=dict(color='black', width=1.5),
            hovertemplate='<b>Price</b>: %{y:.2f}<br>Время: %{x:.0f}с<extra></extra>'
        )
    )

    # ============================================================
    # КОД ОТОБРАЖЕНИЯ СИГНАЛОВ - ЗАКОММЕНТИРОВАН
    # ============================================================
    # # Добавляем сигналы на график цены
    # if len(buy_signals) > 0:
    #     fig_price.add_trace(
    #         go.Scatter(
    #             x=buy_signals['ts_s_rel'],
    #             y=buy_signals['mid_price'],
    #             mode='markers',
    #             name='BUY',
    #             marker=dict(symbol='triangle-up', size=14, color='lime', line=dict(color='darkgreen', width=2)),
    #             hovertemplate='<b>BUY</b><br>Price: %{y:.2f}<br>z-score: %{customdata[0]:.3f}<br>netFlow: %{customdata[1]:.2f}<extra></extra>',
    #             customdata=buy_customdata
    #         )
    #     )
    #
    # if len(sell_signals) > 0:
    #     fig_price.add_trace(
    #         go.Scatter(
    #             x=sell_signals['ts_s_rel'],
    #             y=sell_signals['mid_price'],
    #             mode='markers',
    #             name='SELL',
    #             marker=dict(symbol='triangle-down', size=14, color='red', line=dict(color='darkred', width=2)),
    #             hovertemplate='<b>SELL</b><br>Price: %{y:.2f}<br>z-score: %{customdata[0]:.3f}<br>netFlow: %{customdata[1]:.2f}<extra></extra>',
    #             customdata=sell_customdata
    #         )
    #     )
    # ============================================================

    fig_price.update_layout(
        title=dict(text='<b>📈 Цена (TFI)</b>', font=dict(size=20)),
        xaxis_title='Время (сек)',
        yaxis_title='Price',
        height=700,
        hovermode='x unified',
        dragmode='pan',
        xaxis=dict(
            rangeselector=dict(
                buttons=list([
                    dict(count=5, label="5мин", step="minute", stepmode="backward"),
                    dict(count=15, label="15мин", step="minute", stepmode="backward"),
                    dict(count=30, label="30мин", step="minute", stepmode="backward"),
                    dict(count=1, label="1ч", step="hour", stepmode="backward"),
                    dict(count=6, label="6ч", step="hour", stepmode="backward"),
                    dict(step="all", label="Все")
                ])
            ),
            rangeslider=dict(visible=True, thickness=0.05)
        ),
        legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1)
    )

    fig_price.write_html("tfi_price.html")
    print("✅ Сохранен: tfi_price.html (высота 700px)")

    # --------------------------------------------------------------------
    # 9.2 ИНТЕРАКТИВНЫЙ ГРАФИК: netFlow и mu
    # --------------------------------------------------------------------
    print("\n9.2 Создание интерактивного графика: netFlow и mu...")

    fig_flow = go.Figure()

    fig_flow.add_trace(
        go.Scatter(
            x=df_sampled['ts_s_rel'],
            y=df_sampled['netFlow'],
            mode='lines',
            name='netFlow',
            line=dict(color='blue', width=1.5),
            hovertemplate='<b>netFlow</b>: %{y:.2f}<br>Время: %{x:.0f}с<extra></extra>'
        )
    )

    fig_flow.add_trace(
        go.Scatter(
            x=df_sampled['ts_s_rel'],
            y=df_sampled['mu'],
            mode='lines',
            name='mu (среднее)',
            line=dict(color='orange', width=1.5),
            hovertemplate='<b>mu</b>: %{y:.2f}<br>Время: %{x:.0f}с<extra></extra>'
        )
    )

    fig_flow.add_hline(y=0, line_dash="solid", line_color="gray", line_width=0.5)

    # ============================================================
    # КОД ОТОБРАЖЕНИЯ СИГНАЛОВ - ЗАКОММЕНТИРОВАН
    # ============================================================
    # # Добавляем зоны сигналов
    # if len(buy_signals) > 0:
    #     fig_flow.add_trace(
    #         go.Scatter(
    #             x=buy_signals['ts_s_rel'],
    #             y=buy_signals['netFlow'],
    #             mode='markers',
    #             name='BUY сигнал',
    #             marker=dict(symbol='triangle-up', size=12, color='lime', line=dict(color='darkgreen', width=1.5)),
    #             hovertemplate='<b>BUY</b><br>netFlow: %{y:.2f}<br>z-score: %{customdata[0]:.3f}<extra></extra>',
    #             customdata=buy_customdata
    #         )
    #     )
    #
    # if len(sell_signals) > 0:
    #     fig_flow.add_trace(
    #         go.Scatter(
    #             x=sell_signals['ts_s_rel'],
    #             y=sell_signals['netFlow'],
    #             mode='markers',
    #             name='SELL сигнал',
    #             marker=dict(symbol='triangle-down', size=12, color='red', line=dict(color='darkred', width=1.5)),
    #             hovertemplate='<b>SELL</b><br>netFlow: %{y:.2f}<br>z-score: %{customdata[0]:.3f}<extra></extra>',
    #             customdata=sell_customdata
    #         )
    #     )
    # ============================================================

    fig_flow.update_layout(
        title=dict(text='<b>📊 netFlow и mu (среднее значение)</b>', font=dict(size=20)),
        xaxis_title='Время (сек)',
        yaxis_title='netFlow / mu',
        height=500,
        hovermode='x unified',
        dragmode='pan',
        xaxis=dict(
            rangeselector=dict(
                buttons=list([
                    dict(count=5, label="5мин", step="minute", stepmode="backward"),
                    dict(count=15, label="15мин", step="minute", stepmode="backward"),
                    dict(count=30, label="30мин", step="minute", stepmode="backward"),
                    dict(count=1, label="1ч", step="hour", stepmode="backward"),
                    dict(count=6, label="6ч", step="hour", stepmode="backward"),
                    dict(step="all", label="Все")
                ])
            ),
            rangeslider=dict(visible=True, thickness=0.05)
        ),
        legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1)
    )

    fig_flow.write_html("tfi_flow.html")
    print("✅ Сохранен: tfi_flow.html (высота 500px)")

    # --------------------------------------------------------------------
    # 9.3 ИНТЕРАКТИВНЫЙ ГРАФИК: z-score с порогами
    # --------------------------------------------------------------------
    print("\n9.3 Создание интерактивного графика: z-score с порогами...")

    fig_zscore = go.Figure()

    fig_zscore.add_trace(
        go.Scatter(
            x=df_sampled['ts_s_rel'],
            y=df_sampled['z_score'],
            mode='lines',
            name='z-score',
            line=dict(color='blue', width=1.2),
            hovertemplate='<b>z-score</b>: %{y:.3f}<br>Время: %{x:.0f}с<extra></extra>'
        )
    )

    # Пороги
    fig_zscore.add_hline(
        y=recommended_threshold,
        line_dash="dash",
        line_color="green",
        line_width=2,
        annotation_text=f"рекомендуемый +{recommended_threshold:.2f}",
        annotation_position="bottom right"
    )
    fig_zscore.add_hline(
        y=-recommended_threshold,
        line_dash="dash",
        line_color="green",
        line_width=2,
        annotation_text=f"рекомендуемый -{recommended_threshold:.2f}",
        annotation_position="top right"
    )
    fig_zscore.add_hline(
        y=2.0,
        line_dash="dot",
        line_color="orange",
        line_width=1.5,
        annotation_text="старый +2.0",
        annotation_position="bottom right"
    )
    fig_zscore.add_hline(
        y=-2.0,
        line_dash="dot",
        line_color="orange",
        line_width=1.5,
        annotation_text="старый -2.0",
        annotation_position="top right"
    )
    fig_zscore.add_hline(y=0, line_dash="solid", line_color="gray", line_width=0.5)

    # Добавляем подсветку зон экстремумов
    fig_zscore.add_hrect(
        y0=recommended_threshold,
        y1=df_clean['z_score'].max(),
        fillcolor="rgba(0, 255, 0, 0.05)",
        line_width=0,
        name="Зона BUY"
    )
    fig_zscore.add_hrect(
        y0=df_clean['z_score'].min(),
        y1=-recommended_threshold,
        fillcolor="rgba(255, 0, 0, 0.05)",
        line_width=0,
        name="Зона SELL"
    )

    # ============================================================
    # КОД ОТОБРАЖЕНИЯ СИГНАЛОВ - ЗАКОММЕНТИРОВАН
    # ============================================================
    # # Добавляем сигналы на график z-score
    # if len(buy_signals) > 0:
    #     fig_zscore.add_trace(
    #         go.Scatter(
    #             x=buy_signals['ts_s_rel'],
    #             y=buy_signals['z_score'],
    #             mode='markers',
    #             name='BUY',
    #             marker=dict(symbol='triangle-up', size=10, color='lime', line=dict(color='darkgreen', width=1.5)),
    #             hovertemplate='<b>BUY</b><br>z-score: %{y:.3f}<extra></extra>'
    #         )
    #     )
    #
    # if len(sell_signals) > 0:
    #     fig_zscore.add_trace(
    #         go.Scatter(
    #             x=sell_signals['ts_s_rel'],
    #             y=sell_signals['z_score'],
    #             mode='markers',
    #             name='SELL',
    #             marker=dict(symbol='triangle-down', size=10, color='red', line=dict(color='darkred', width=1.5)),
    #             hovertemplate='<b>SELL</b><br>z-score: %{y:.3f}<extra></extra>'
    #         )
    #     )
    # ============================================================

    fig_zscore.update_layout(
        title=dict(text='<b>📊 Z-score с порогами</b>', font=dict(size=20)),
        xaxis_title='Время (сек)',
        yaxis_title='z-score',
        height=500,
        hovermode='x unified',
        dragmode='pan',
        xaxis=dict(
            rangeselector=dict(
                buttons=list([
                    dict(count=5, label="5мин", step="minute", stepmode="backward"),
                    dict(count=15, label="15мин", step="minute", stepmode="backward"),
                    dict(count=30, label="30мин", step="minute", stepmode="backward"),
                    dict(count=1, label="1ч", step="hour", stepmode="backward"),
                    dict(count=6, label="6ч", step="hour", stepmode="backward"),
                    dict(step="all", label="Все")
                ])
            ),
            rangeslider=dict(visible=True, thickness=0.05)
        ),
        legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1)
    )

    fig_zscore.write_html("tfi_zscore.html")
    print("✅ Сохранен: tfi_zscore.html (высота 500px)")

    # --------------------------------------------------------------------
    # 9.4 ИНТЕРАКТИВНЫЙ ГРАФИК: sigma (волатильность)
    # --------------------------------------------------------------------
    print("\n9.4 Создание интерактивного графика: sigma...")

    fig_sigma = go.Figure()

    fig_sigma.add_trace(
        go.Scatter(
            x=df_sampled['ts_s_rel'],
            y=df_sampled['sigma'],
            mode='lines',
            name='sigma',
            line=dict(color='#ff7f0e', width=1.5),
            fill='tozeroy',
            fillcolor='rgba(255, 165, 0, 0.2)',
            hovertemplate='<b>sigma</b>: %{y:.6f}<br>Время: %{x:.0f}с<extra></extra>'
        )
    )

    # Медиана
    median_sigma = df_clean['sigma'].median()
    fig_sigma.add_hline(
        y=median_sigma,
        line_dash="dash",
        line_color="red",
        line_width=1.5,
        annotation_text=f"медиана = {median_sigma:.6f}"
    )

    # Квартили
    q25 = df_clean['sigma'].quantile(0.25)
    q75 = df_clean['sigma'].quantile(0.75)
    fig_sigma.add_hrect(
        y0=q25,
        y1=q75,
        fillcolor="rgba(255, 0, 0, 0.05)",
        line_width=0,
        name="Межквартильный размах"
    )

    # ============================================================
    # КОД ОТОБРАЖЕНИЯ СИГНАЛОВ - ЗАКОММЕНТИРОВАН
    # ============================================================
    # # Добавляем сигналы
    # if len(buy_signals) > 0:
    #     fig_sigma.add_trace(
    #         go.Scatter(
    #             x=buy_signals['ts_s_rel'],
    #             y=buy_signals['sigma'],
    #             mode='markers',
    #             name='BUY',
    #             marker=dict(symbol='triangle-up', size=10, color='lime', line=dict(color='darkgreen', width=1.5)),
    #             hovertemplate='<b>BUY</b><br>sigma: %{y:.6f}<extra></extra>'
    #         )
    #     )
    #
    # if len(sell_signals) > 0:
    #     fig_sigma.add_trace(
    #         go.Scatter(
    #             x=sell_signals['ts_s_rel'],
    #             y=sell_signals['sigma'],
    #             mode='markers',
    #             name='SELL',
    #             marker=dict(symbol='triangle-down', size=10, color='red', line=dict(color='darkred', width=1.5)),
    #             hovertemplate='<b>SELL</b><br>sigma: %{y:.6f}<extra></extra>'
    #         )
    #     )
    # ============================================================

    fig_sigma.update_layout(
        title=dict(text='<b>📊 Sigma (волатильность netFlow)</b>', font=dict(size=20)),
        xaxis_title='Время (сек)',
        yaxis_title='sigma',
        height=400,
        hovermode='x unified',
        dragmode='pan',
        xaxis=dict(
            rangeselector=dict(
                buttons=list([
                    dict(count=5, label="5мин", step="minute", stepmode="backward"),
                    dict(count=15, label="15мин", step="minute", stepmode="backward"),
                    dict(count=30, label="30мин", step="minute", stepmode="backward"),
                    dict(count=1, label="1ч", step="hour", stepmode="backward"),
                    dict(count=6, label="6ч", step="hour", stepmode="backward"),
                    dict(step="all", label="Все")
                ])
            ),
            rangeslider=dict(visible=True, thickness=0.05)
        ),
        legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1)
    )

    fig_sigma.write_html("tfi_sigma.html")
    print("✅ Сохранен: tfi_sigma.html (высота 400px)")

    # --------------------------------------------------------------------
    # 9.5 ИНТЕРАКТИВНЫЙ ГРАФИК: Распределение z-score (без сэмплирования)
    # --------------------------------------------------------------------
    print("\n9.5 Создание интерактивного графика: Распределение z-score...")

    fig_dist = go.Figure()

    # Гистограмма (используем все данные)
    fig_dist.add_trace(
        go.Histogram(
            x=df_clean['z_score'],
            nbinsx=80,
            name='Распределение',
            opacity=0.7,
            marker=dict(color='steelblue', line=dict(color='black', width=0.5)),
            hovertemplate='<b>z-score</b>: %{x:.3f}<br><b>Частота</b>: %{y}<extra></extra>'
        )
    )

    # Нормальное распределение
    x_norm = np.linspace(df_clean['z_score'].min(), df_clean['z_score'].max(), 100)
    bin_width = (df_clean['z_score'].max() - df_clean['z_score'].min()) / 80
    y_norm = norm.pdf(x_norm, 0, 1) * len(df_clean) * bin_width

    fig_dist.add_trace(
        go.Scatter(
            x=x_norm,
            y=y_norm,
            mode='lines',
            name='N(0,1)',
            line=dict(color='red', width=2.5),
            hovertemplate='<b>N(0,1)</b>: %{y:.0f}<extra></extra>'
        )
    )

    # Вертикальные линии порогов
    fig_dist.add_vline(
        x=recommended_threshold,
        line_dash="dash",
        line_color="green",
        line_width=2.5,
        annotation_text=f"новый +{recommended_threshold:.2f}",
        annotation_position="top"
    )
    fig_dist.add_vline(
        x=-recommended_threshold,
        line_dash="dash",
        line_color="green",
        line_width=2.5,
        annotation_text=f"новый -{recommended_threshold:.2f}",
        annotation_position="bottom"
    )
    fig_dist.add_vline(
        x=2.0,
        line_dash="dot",
        line_color="orange",
        line_width=2,
        annotation_text="старый +2.0",
        annotation_position="top"
    )
    fig_dist.add_vline(
        x=-2.0,
        line_dash="dot",
        line_color="orange",
        line_width=2,
        annotation_text="старый -2.0",
        annotation_position="bottom"
    )

    # Статистика в аннотации
    stats_text = (f"<b>Статистика z-score:</b><br>"
                  f"Среднее: {df_clean['z_score'].mean():.3f}<br>"
                  f"СКО: {df_clean['z_score'].std():.3f}<br>"
                  f"Min: {df_clean['z_score'].min():.3f}<br>"
                  f"Max: {df_clean['z_score'].max():.3f}<br>"
                  f"Сигналов: {len(buy_signals) + len(sell_signals)}")

    fig_dist.add_annotation(
        x=0.98,
        y=0.95,
        xref="paper",
        yref="paper",
        text=stats_text,
        showarrow=False,
        font=dict(size=12),
        bgcolor="white",
        bordercolor="black",
        borderwidth=1.5,
        align="left"
    )

    fig_dist.update_layout(
        title=dict(text='<b>📊 Распределение z-score с порогами</b>', font=dict(size=20)),
        xaxis_title='z-score',
        yaxis_title='Частота',
        height=450,
        hovermode='x unified',
        dragmode='pan',
        barmode='overlay',
        legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1)
    )

    fig_dist.write_html("tfi_distribution.html")
    print("✅ Сохранен: tfi_distribution.html (высота 450px)")

    # --------------------------------------------------------------------
    # 9.6 СВОДНЫЙ HTML С ГРУППОЙ ГРАФИКОВ
    # --------------------------------------------------------------------
    print("\n9.6 Создание сводного HTML с группой графиков...")

    html_content = '''<!DOCTYPE html>
    <html>
    <head>
        <title>TFI Анализ - Сводка графиков</title>
        <style>
            body {
                font-family: 'Segoe UI', Arial, sans-serif;
                background: #f0f2f5;
                margin: 0;
                padding: 20px;
            }
            h1 {
                color: #1a1a2e;
                text-align: center;
                font-size: 28px;
                margin-bottom: 10px;
            }
            .subtitle {
                text-align: center;
                color: #666;
                font-size: 14px;
                margin-bottom: 30px;
            }
            .graph-container {
                background: white;
                border-radius: 12px;
                box-shadow: 0 4px 20px rgba(0,0,0,0.08);
                margin: 25px 0;
                padding: 20px;
                transition: box-shadow 0.3s ease;
            }
            .graph-container:hover {
                box-shadow: 0 6px 30px rgba(0,0,0,0.12);
            }
            .graph-container h2 {
                color: #1a1a2e;
                font-size: 18px;
                margin-top: 0;
                margin-bottom: 15px;
                padding-bottom: 10px;
                border-bottom: 2px solid #f0f2f5;
            }
            .graph-container iframe {
                width: 100%;
                border: none;
                border-radius: 8px;
                background: white;
            }
            .stats {
                background: white;
                border-radius: 12px;
                padding: 25px;
                margin: 25px 0;
                box-shadow: 0 4px 20px rgba(0,0,0,0.08);
            }
            .stats h2 {
                color: #1a1a2e;
                font-size: 20px;
                margin-top: 0;
                margin-bottom: 15px;
            }
            .stats-grid {
                display: grid;
                grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
                gap: 15px;
            }
            .stats-item {
                background: #f8f9fa;
                padding: 12px 16px;
                border-radius: 8px;
                border-left: 4px solid #4a90d9;
            }
            .stats-item .label {
                font-size: 12px;
                color: #888;
                text-transform: uppercase;
                letter-spacing: 0.5px;
            }
            .stats-item .value {
                font-size: 18px;
                font-weight: 600;
                color: #1a1a2e;
                margin-top: 2px;
            }
            .stats-item .value.positive { color: #2e7d32; }
            .stats-item .value.negative { color: #c62828; }
            .stats-item .value.highlight { color: #e65100; }
            .footer {
                text-align: center;
                color: #999;
                font-size: 13px;
                margin-top: 40px;
                padding: 20px;
                border-top: 1px solid #e0e0e0;
            }
            .footer kbd {
                background: #f0f0f0;
                padding: 2px 8px;
                border-radius: 4px;
                border: 1px solid #ccc;
                font-size: 12px;
            }
            .info-note {
                background: #fff3e0;
                padding: 10px 20px;
                border-radius: 8px;
                margin: 15px 0;
                border-left: 4px solid #ff9800;
                font-size: 14px;
            }
        </style>
        <script>
            document.addEventListener('DOMContentLoaded', function() {
                document.querySelectorAll('iframe').forEach(function(iframe) {
                    iframe.addEventListener('load', function() {
                        try {
                            const content = iframe.contentDocument || iframe.contentWindow.document;
                            const plot = content.querySelector('.plotly-graph-div');
                            if (plot && plot._fullLayout) {
                                Plotly.Plots.resize(plot);
                            }
                        } catch(e) {}
                    });
                });

                window.addEventListener('resize', function() {
                    document.querySelectorAll('iframe').forEach(function(iframe) {
                        try {
                            const content = iframe.contentDocument || iframe.contentWindow.document;
                            const plot = content.querySelector('.plotly-graph-div');
                            if (plot && plot._fullLayout) {
                                Plotly.Plots.resize(plot);
                            }
                        } catch(e) {}
                    });
                });
            });
        </script>
    </head>
    <body>
        <h1>📊 TFI Анализ — Интерактивный дашборд</h1>
        <p class="subtitle">Все графики интерактивны • Используйте <kbd>Ctrl + Колесо</kbd> для масштабирования</p>

        <div class="info-note">
            📌 <b>Оптимизация производительности:</b> Для ускорения загрузки отображается каждая ''' + str(SAMPLE_STEP) + '''-я точка.
        </div>

        <div class="stats">
            <h2>📈 Ключевые метрики</h2>
            <div class="stats-grid">
                <div class="stats-item">
                    <div class="label">📊 Всего записей</div>
                    <div class="value">''' + f"{len(df_clean):,}" + '''</div>
                </div>
                <div class="stats-item" style="border-left-color: #2e7d32;">
                    <div class="label">🟢 BUY сигналов</div>
                    <div class="value positive">''' + f"{len(buy_signals)} ({len(buy_signals)/len(df_clean)*100:.2f}%)" + '''</div>
                </div>
                <div class="stats-item" style="border-left-color: #c62828;">
                    <div class="label">🔴 SELL сигналов</div>
                    <div class="value negative">''' + f"{len(sell_signals)} ({len(sell_signals)/len(df_clean)*100:.2f}%)" + '''</div>
                </div>
                <div class="stats-item">
                    <div class="label">📉 Z-score среднее</div>
                    <div class="value">''' + f"{df_clean['z_score'].mean():.3f}" + '''</div>
                </div>
                <div class="stats-item">
                    <div class="label">📊 Z-score СКО</div>
                    <div class="value">''' + f"{df_clean['z_score'].std():.3f}" + '''</div>
                </div>
                <div class="stats-item" style="border-left-color: #e65100;">
                    <div class="label">🎯 Рекомендуемый порог</div>
                    <div class="value highlight">''' + f"{recommended_threshold:.2f}" + '''</div>
                </div>
                <div class="stats-item" style="border-left-color: #4a90d9;">
                    <div class="label">🎯 Точность сигналов</div>
                    <div class="value"></div>
                </div>
                <div class="stats-item" style="border-left-color: #ff6f00;">
                    <div class="label">📈 Sigma средняя</div>
                    <div class="value">''' + f"{df_clean['sigma'].mean():.6f}" + '''</div>
                </div>
            </div>
        </div>

        <div class="graph-container">
            <h2>📈 График 1: Цена</h2>
            <iframe src="tfi_price.html" height="700"></iframe>
        </div>

        <div class="graph-container">
            <h2>📊 График 2: netFlow и mu</h2>
            <iframe src="tfi_flow.html" height="500"></iframe>
        </div>

        <div class="graph-container">
            <h2>📊 График 3: Z-score с порогами</h2>
            <iframe src="tfi_zscore.html" height="500"></iframe>
        </div>

        <div class="graph-container">
            <h2>📊 График 4: Sigma (волатильность netFlow)</h2>
            <iframe src="tfi_sigma.html" height="400"></iframe>
        </div>

        <div class="graph-container">
            <h2>📊 График 5: Распределение z-score</h2>
            <iframe src="tfi_distribution.html" height="450"></iframe>
        </div>

        <div class="footer">
            <p>📌 Сгенерировано автоматически • Все графики интерактивны • <kbd>Ctrl + Колесо</kbd> для масштабирования</p>
            <p style="font-size: 11px; color: #bbb;">TFI Analysis Tool v2.0 (без отображения сигналов)</p>
        </div>
    </body>
    </html>
    '''

    with open('tfi_dashboard.html', 'w', encoding='utf-8') as f:
        f.write(html_content)
    print("✅ Сохранен: tfi_dashboard.html")

    print("\n" + "=" * 60)
    print("ИНТЕРАКТИВНЫЕ ГРАФИКИ СОЗДАНЫ")
    print("=" * 60)
    print(f"\n📌 Откройте в браузере: tfi_dashboard.html")
    print(f"📌 Сэмплирование: каждая {SAMPLE_STEP}-я точка ({len(df_sampled):,} точек из {len(df_clean):,})")
    print("📌 Сигналы НЕ отображаются на графиках")
    print("📌 Для масштабирования: Ctrl + Колесо мыши")
    print("📌 Наводите на графики для просмотра значений")
    print("=" * 60)

else:
    print("\n⚠️ Plotly не установлен. Пропускаем создание интерактивных графиков.")
    print("   Установите: pip install plotly")
