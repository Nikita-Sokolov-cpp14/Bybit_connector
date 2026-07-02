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
def analyze_signals(df, signal_type, horizon_sec=120, target_move=0.0020, max_drawdown=0.0040):
    """
    Анализ качества сигналов TFI

    Параметры:
    - df: DataFrame с данными
    - signal_type: 1 или 2
    - horizon_sec: горизонт удержания (сек)
    - target_move: целевое движение (0.0020 = 0.20%)
    - max_drawdown: максимальная просадка для стоп-лосса (0.0040 = 0.40%)

    Возвращает: словарь с метриками
    """
    signals = df[df['signal'] == signal_type].copy()

    if len(signals) == 0:
        return {
            'good': 0, 'false': 0, 'stopped_out': 0,
            'accuracy': 0, 'total_signals': 0, 'valid_signals': 0,
            'avg_move': 0, 'median_move': 0, 'win_ratio': 0,
            'avg_profit': 0, 'avg_loss': 0, 'profit_factor': 0
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

        # Берем будущие данные
        future = df[(df['ts'] >= entry_time) &
                    (df['ts'] <= entry_time + horizon_sec * 1000)]

        if len(future) < 2:
            continue

        max_price = future['mid_price'].max()
        min_price = future['mid_price'].min()

        if signal_type == 1:
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
        'avg_profit': avg_profit,
        'avg_loss': avg_loss,
        'profit_factor': profit_factor,
        'moves': moves
    }

# ============================================
# 4. АНАЛИЗ СИГНАЛОВ ПРИ ТЕКУЩИХ ПАРАМЕТРАХ
# ============================================
print("\n" + "=" * 60)
print("АНАЛИЗ СИГНАЛОВ (z_thresh = 2.0)")
print("=" * 60)

TARGET_MOVE = 0.0020  # 0.20%
MAX_DD = 0.0040       # 0.40%
HORIZON = 120         # 2 минуты

buy_res = analyze_signals(df_clean, 1, HORIZON, TARGET_MOVE, MAX_DD)
sell_res = analyze_signals(df_clean, 2, HORIZON, TARGET_MOVE, MAX_DD)

print(f"\n=== BUY СИГНАЛЫ ===")
print(f"Всего: {buy_res['total_signals']}")
print(f"  Успешных: {buy_res['good']} ({buy_res['accuracy']:.2%})")
print(f"  Неудачных: {buy_res['false']}")
print(f"  Стоп-лосс: {buy_res['stopped_out']}")
print(f"Среднее движение: {buy_res['avg_move']:.4%}")
print(f"Медианное движение: {buy_res['median_move']:.4%}")
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
print(f"Средний профит: {sell_res['avg_profit']:.4%}")
print(f"Средний убыток: {sell_res['avg_loss']:.4%}")
print(f"Profit Factor: {sell_res['profit_factor']:.2f}")

# Общая точность
total_signals = buy_res['total_signals'] + sell_res['total_signals']
total_good = buy_res['good'] + sell_res['good']
total_acc = total_good / total_signals if total_signals > 0 else 0
print(f"\n=== ОБЩАЯ ТОЧНОСТЬ ===")
print(f"Всего сигналов: {total_signals}")
print(f"Успешных: {total_good} ({total_acc:.2%})")

# ============================================
# 5. ОПТИМИЗАЦИЯ ПОРОГА Z-SCORE
# ============================================
print("\n" + "=" * 60)
print("ОПТИМИЗАЦИЯ ПОРОГА Z-SCORE")
print("=" * 60)

def optimize_z_threshold(df, train_ratio=0.7, target_move=0.0020,
                         max_drawdown=0.0040, horizon_sec=120):
    """
    Оптимизация порога z-score с разделением на train/test
    """
    # Разделяем данные
    split_idx = int(len(df) * train_ratio)
    train_df = df.iloc[:split_idx].copy()
    test_df = df.iloc[split_idx:].copy()

    results = []

    print(f"\nTrain период: {len(train_df)} записей")
    print(f"Test период: {len(test_df)} записей")
    print("\nПеребор порогов z-score...")
    print("-" * 60)

    # Перебираем пороги от 1.0 до 4.0 с шагом 0.1
    for z_thresh in np.arange(1.0, 4.1, 0.1):
        # Генерируем сигналы на train
        train_buy = train_df[train_df['z_score'] >= z_thresh]
        train_sell = train_df[train_df['z_score'] <= -z_thresh]

        # Оцениваем на train
        buy_res = analyze_signals(train_buy, 1, horizon_sec, target_move, max_drawdown)
        sell_res = analyze_signals(train_sell, 2, horizon_sec, target_move, max_drawdown)

        train_total = buy_res['total_signals'] + sell_res['total_signals']
        train_good = buy_res['good'] + sell_res['good']
        train_acc = train_good / train_total if train_total > 0 else 0

        # Оцениваем на test
        test_buy = test_df[test_df['z_score'] >= z_thresh]
        test_sell = test_df[test_df['z_score'] <= -z_thresh]

        buy_res_test = analyze_signals(test_buy, 1, horizon_sec, target_move, max_drawdown)
        sell_res_test = analyze_signals(test_sell, 2, horizon_sec, target_move, max_drawdown)

        test_total = buy_res_test['total_signals'] + sell_res_test['total_signals']
        test_good = buy_res_test['good'] + sell_res_test['good']
        test_acc = test_good / test_total if test_total > 0 else 0

        # Сохраняем только если есть сигналы
        if train_total >= 5 and test_total >= 5:
            results.append({
                'z_thresh': z_thresh,
                'train_acc': train_acc,
                'test_acc': test_acc,
                'train_total': train_total,
                'test_total': test_total,
                'train_good': train_good,
                'test_good': test_good,
                'train_buy': buy_res['total_signals'],
                'train_sell': sell_res['total_signals'],
                'test_buy': buy_res_test['total_signals'],
                'test_sell': sell_res_test['total_signals'],
                'train_buy_acc': buy_res['accuracy'],
                'train_sell_acc': sell_res['accuracy'],
                'test_buy_acc': buy_res_test['accuracy'],
                'test_sell_acc': sell_res_test['accuracy']
            })

            print(f"z={z_thresh:.1f}: train_acc={train_acc:.2%} ({train_total} sig), "
                  f"test_acc={test_acc:.2%} ({test_total} sig)")

    if not results:
        print("Не найдено комбинаций с достаточным количеством сигналов!")
        return None, None

    # Находим лучший по test_acc
    results_df = pd.DataFrame(results)
    best = results_df.loc[results_df['test_acc'].idxmax()]

    return best, results_df

# Запуск оптимизации
best_params, opt_results = optimize_z_threshold(
    df_clean,
    train_ratio=0.7,
    target_move=TARGET_MOVE,
    max_drawdown=MAX_DD,
    horizon_sec=HORIZON
)

if best_params is not None:
    print("\n" + "=" * 60)
    print("РЕЗУЛЬТАТЫ ОПТИМИЗАЦИИ")
    print("=" * 60)
    print(f"\nОптимальный порог z-score: {best_params['z_thresh']:.1f}")
    print(f"\nTrain accuracy: {best_params['train_acc']:.2%} ({best_params['train_total']} сигналов)")
    print(f"  BUY: {best_params['train_buy']} сигналов, accuracy={best_params['train_buy_acc']:.2%}")
    print(f"  SELL: {best_params['train_sell']} сигналов, accuracy={best_params['train_sell_acc']:.2%}")
    print(f"\nTest accuracy: {best_params['test_acc']:.2%} ({best_params['test_total']} сигналов)")
    print(f"  BUY: {best_params['test_buy']} сигналов, accuracy={best_params['test_buy_acc']:.2%}")
    print(f"  SELL: {best_params['test_sell']} сигналов, accuracy={best_params['test_sell_acc']:.2%}")

    # Топ-5 комбинаций
    print("\nТоп-5 комбинаций:")
    print(opt_results.head(5)[['z_thresh', 'train_acc', 'test_acc',
                                'train_total', 'test_total']].to_string(index=False))
else:
    print("\nОптимизация не удалась!")

# ============================================
# 6. ВИЗУАЛИЗАЦИЯ
# ============================================
print("\n" + "=" * 60)
print("ПОСТРОЕНИЕ ГРАФИКОВ")
print("=" * 60)

fig = plt.figure(figsize=(16, 12))

# 6.1 Распределение z-score
ax1 = plt.subplot(3, 3, 1)
ax1.hist(df_clean['z_score'], bins=80, alpha=0.7, color='steelblue', edgecolor='black', density=True)
# Нормальное распределение
x = np.linspace(df_clean['z_score'].min(), df_clean['z_score'].max(), 100)
ax1.plot(x, norm.pdf(x, 0, 1), 'r-', linewidth=2, label='N(0,1)')
ax1.axvline(2.0, color='g', linestyle='--', linewidth=2, label='threshold ±2.0')
ax1.axvline(-2.0, color='g', linestyle='--', linewidth=2)
if 'z_thresh' in locals():
    ax1.axvline(best_params['z_thresh'], color='orange', linestyle=':', linewidth=2,
                label=f'optimal {best_params["z_thresh"]:.1f}')
    ax1.axvline(-best_params['z_thresh'], color='orange', linestyle=':', linewidth=2)
ax1.set_title('Распределение z-score', fontsize=12)
ax1.set_xlabel('z-score')
ax1.set_ylabel('Плотность')
ax1.legend(fontsize=8)
ax1.grid(True, alpha=0.3)

# 6.2 QQ-plot z-score
ax2 = plt.subplot(3, 3, 2)
stats.probplot(df_clean['z_score'][df_clean['dataSize'] >= 20], dist="norm", plot=ax2)
ax2.set_title('QQ-plot z-score', fontsize=12)
ax2.grid(True, alpha=0.3)

# 6.3 Распределение sigma
ax3 = plt.subplot(3, 3, 3)
ax3.hist(df_clean['sigma'], bins=50, alpha=0.7, color='coral', edgecolor='black')
ax3.axvline(df_clean['sigma'].median(), color='red', linestyle='--',
            label=f'медиана = {df_clean["sigma"].median():.6f}')
ax3.set_title('Распределение sigma', fontsize=12)
ax3.set_xlabel('sigma')
ax3.set_ylabel('Частота')
ax3.legend(fontsize=8)
ax3.grid(True, alpha=0.3)

# 6.4 Зависимость z-score от shortWin
ax4 = plt.subplot(3, 3, 4)
scatter = ax4.scatter(df_clean['shortWin'], df_clean['z_score'],
                      c=df_clean['sigma'], cmap='viridis', alpha=0.5, s=3)
ax4.axhline(2.0, color='g', linestyle='--', linewidth=1)
ax4.axhline(-2.0, color='g', linestyle='--', linewidth=1)
ax4.set_title('z-score vs shortWin (цвет = sigma)', fontsize=12)
ax4.set_xlabel('Количество сделок в 300 мс (shortWin)')
ax4.set_ylabel('z-score')
plt.colorbar(scatter, ax=ax4, label='sigma')

# 6.5 Зависимость accuracy от shortWin (для сигналов)
ax5 = plt.subplot(3, 3, 5)
# Группируем сигналы по shortWin
signals_df = df_clean[df_clean['signal'] != 0].copy()
if len(signals_df) > 0:
    bins = np.arange(0, signals_df['shortWin'].max() + 5, 5)
    signals_df['shortWin_bin'] = pd.cut(signals_df['shortWin'], bins=bins)

    # Считаем accuracy для каждой группы
    acc_by_win = []
    for name, group in signals_df.groupby('shortWin_bin'):
        if len(group) >= 10:
            buy_acc = analyze_signals(group[group['signal'] == 1], 1,
                                      HORIZON, TARGET_MOVE, MAX_DD)['accuracy']
            sell_acc = analyze_signals(group[group['signal'] == 2], 2,
                                       HORIZON, TARGET_MOVE, MAX_DD)['accuracy']
            acc_by_win.append({
                'bin': name,
                'mid': (name.left + name.right) / 2,
                'accuracy': (buy_acc + sell_acc) / 2,
                'count': len(group)
            })

    if acc_by_win:
        acc_df = pd.DataFrame(acc_by_win)
        ax5.plot(acc_df['mid'], acc_df['accuracy'], 'o-', color='steelblue', linewidth=2)
        ax5.axhline(0.5, color='red', linestyle='--', label='random')
        ax5.set_title('Точность vs shortWin', fontsize=12)
        ax5.set_xlabel('Количество сделок в 300 мс')
        ax5.set_ylabel('Accuracy')
        ax5.grid(True, alpha=0.3)
        ax5.legend()

# 6.6 Зависимость точности от sigma
ax6 = plt.subplot(3, 3, 6)
if len(signals_df) > 0:
    bins = np.linspace(signals_df['sigma'].min(), signals_df['sigma'].max(), 20)
    signals_df['sigma_bin'] = pd.cut(signals_df['sigma'], bins=bins)

    acc_by_sigma = []
    for name, group in signals_df.groupby('sigma_bin'):
        if len(group) >= 10:
            buy_acc = analyze_signals(group[group['signal'] == 1], 1,
                                      HORIZON, TARGET_MOVE, MAX_DD)['accuracy']
            sell_acc = analyze_signals(group[group['signal'] == 2], 2,
                                       HORIZON, TARGET_MOVE, MAX_DD)['accuracy']
            acc_by_sigma.append({
                'bin': name,
                'mid': (name.left + name.right) / 2,
                'accuracy': (buy_acc + sell_acc) / 2,
                'count': len(group)
            })

    if acc_by_sigma:
        acc_df = pd.DataFrame(acc_by_sigma)
        ax6.plot(acc_df['mid'], acc_df['accuracy'], 'o-', color='coral', linewidth=2)
        ax6.axhline(0.5, color='red', linestyle='--', label='random')
        ax6.set_title('Точность vs sigma', fontsize=12)
        ax6.set_xlabel('sigma')
        ax6.set_ylabel('Accuracy')
        ax6.grid(True, alpha=0.3)
        ax6.legend()

# 6.7 Цена с сигналами (выборка)
ax7 = plt.subplot(3, 3, 7)
# Берем участок с сигналами
signal_indices = df_clean[df_clean['signal'] != 0].index
if len(signal_indices) > 0:
    start_idx = max(0, signal_indices[0] - 100)
    end_idx = min(len(df_clean), signal_indices[-1] + 100)
    plot_df = df_clean.iloc[start_idx:end_idx]

    ax7.plot(plot_df['ts_s_rel'], plot_df['mid_price'], 'k-', alpha=0.7, linewidth=1)

    # Отмечаем сигналы
    buy_plot = plot_df[plot_df['signal'] == 1]
    sell_plot = plot_df[plot_df['signal'] == 2]

    ax7.scatter(buy_plot['ts_s_rel'], buy_plot['mid_price'],
                marker='^', s=80, c='green', label='BUY', zorder=5)
    ax7.scatter(sell_plot['ts_s_rel'], sell_plot['mid_price'],
                marker='v', s=80, c='red', label='SELL', zorder=5)

    ax7.set_title('Цена с сигналами TFI', fontsize=12)
    ax7.set_xlabel('Время (сек)')
    ax7.set_ylabel('Mid Price')
    ax7.legend(fontsize=8)
    ax7.grid(True, alpha=0.3)

# 6.8 Распределение сигналов по времени
ax8 = plt.subplot(3, 3, 8)
if len(signals_df) > 0:
    signals_df['hour'] = pd.to_datetime(signals_df['ts'], unit='ms').dt.hour

    # Считаем сигналы по часам
    hourly_counts = signals_df.groupby(['hour', 'signal']).size().unstack(fill_value=0)

    if 1 in hourly_counts.columns and 2 in hourly_counts.columns:
        width = 0.35
        x = np.arange(len(hourly_counts.index))
        ax8.bar(x - width/2, hourly_counts[1], width, label='BUY', color='green', alpha=0.7)
        ax8.bar(x + width/2, hourly_counts[2], width, label='SELL', color='red', alpha=0.7)
        ax8.set_xticks(x)
        ax8.set_xticklabels(hourly_counts.index)
        ax8.set_title('Сигналы по часам', fontsize=12)
        ax8.set_xlabel('Час (UTC)')
        ax8.set_ylabel('Количество сигналов')
        ax8.legend(fontsize=8)
        ax8.grid(True, alpha=0.3)

# 6.9 Результаты оптимизации
ax9 = plt.subplot(3, 3, 9)
if opt_results is not None and len(opt_results) > 0:
    ax9.plot(opt_results['z_thresh'], opt_results['train_acc'], 'o-',
             label='Train', color='blue', linewidth=2)
    ax9.plot(opt_results['z_thresh'], opt_results['test_acc'], 's-',
             label='Test', color='red', linewidth=2)
    if 'z_thresh' in locals():
        ax9.axvline(best_params['z_thresh'], color='green', linestyle='--',
                    label=f'best = {best_params["z_thresh"]:.1f}')
    ax9.set_title('Оптимизация порога z-score', fontsize=12)
    ax9.set_xlabel('Порог z-score')
    ax9.set_ylabel('Accuracy')
    ax9.legend(fontsize=8)
    ax9.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig('tfi_analysis.png', dpi=150, bbox_inches='tight')
print("\nГрафик сохранен: tfi_analysis.png")
plt.show()

# ============================================
# 7. ДОПОЛНИТЕЛЬНЫЙ АНАЛИЗ: РАЗНЫЕ ГОРИЗОНТЫ
# ============================================
print("\n" + "=" * 60)
print("АНАЛИЗ ДЛЯ РАЗНЫХ ГОРИЗОНТОВ")
print("=" * 60)

horizons = [60, 120, 180]
targets = [0.0015, 0.0020, 0.0025]

print("\nAccuracy для разных горизонтов и целей:")
print("-" * 60)
print(f"{'Горизонт':>10} | {'Цель':>8} | {'BUY':>8} | {'SELL':>8} | {'Total':>8}")
print("-" * 60)

for h in horizons:
    for t in targets:
        buy_res_h = analyze_signals(df_clean[df_clean['signal'] == 1], 1, h, t, MAX_DD)
        sell_res_h = analyze_signals(df_clean[df_clean['signal'] == 2], 2, h, t, MAX_DD)
        total_acc = (buy_res_h['good'] + sell_res_h['good']) / \
                    (buy_res_h['total_signals'] + sell_res_h['total_signals']) \
                    if (buy_res_h['total_signals'] + sell_res_h['total_signals']) > 0 else 0
        print(f"{h:>10}s | {t:>7.2%} | {buy_res_h['accuracy']:>7.2%} | "
              f"{sell_res_h['accuracy']:>7.2%} | {total_acc:>7.2%}")

# ============================================
# 8. АНАЛИЗ ЛУЧШИХ/ХУДШИХ СИГНАЛОВ
# ============================================
print("\n" + "=" * 60)
print("АНАЛИЗ КАЧЕСТВА СИГНАЛОВ")
print("=" * 60)

# Смотрим на распределение движений после сигналов
if len(signals_df) > 0:
    # Получаем движения для всех сигналов
    all_moves = []
    for signal_type in [1, 2]:
        sig_df = signals_df[signals_df['signal'] == signal_type]
        for idx, row in sig_df.iterrows():
            entry = row['mid_price']
            future = df_clean[(df_clean['ts'] >= row['ts']) &
                              (df_clean['ts'] <= row['ts'] + HORIZON * 1000)]
            if len(future) > 0:
                if signal_type == 1:
                    move = (future['mid_price'].max() - entry) / entry
                else:
                    move = (entry - future['mid_price'].min()) / entry
                all_moves.append(move)

    if all_moves:
        print(f"\nРаспределение движений после сигналов:")
        print(f"  Среднее: {np.mean(all_moves):.4%}")
        print(f"  Медиана: {np.median(all_moves):.4%}")
        print(f"  Стандартное отклонение: {np.std(all_moves):.4%}")
        print(f"  Min: {np.min(all_moves):.4%}")
        print(f"  Max: {np.max(all_moves):.4%}")
        print(f"  Доля >= 0.20%: {np.mean(np.array(all_moves) >= TARGET_MOVE):.2%}")

print("\n" + "=" * 60)
print("АНАЛИЗ ЗАВЕРШЕН")
print("=" * 60)
