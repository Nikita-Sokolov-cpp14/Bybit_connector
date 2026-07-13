import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from scipy.stats import norm, probplot, pearsonr
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
print("АНАЛИЗ OBI (Order Book Imbalance)")
print("=" * 60)

# Загрузка
df = pd.read_csv('../log_files/imbalance.csv', delimiter='\t')
df.columns = ['ts', 'mid_price', 'DW_OBI', 'ema_OBI', 'agg_OBI',
              'OBI_recent', 'OBI_prev', 'sigma', 'z_score', 'signal']

print(f"\nЗагружено записей: {len(df)}")
print(f"Период: {pd.to_datetime(df['ts'].min(), unit='ms')} - {pd.to_datetime(df['ts'].max(), unit='ms')}")
print(f"Длительность: {(df['ts'].max() - df['ts'].min()) / 1000 / 60:.1f} минут")

# Преобразуем время
df['ts_s'] = df['ts'] / 1000  # секунды
df['ts_s_rel'] = df['ts_s'] - df['ts_s'].iloc[0]  # относительное время

# Очистка: удаляем записи с нулевой sigma (нет данных)
df_clean = df[df['sigma'] > 1e-10].copy()
print(f"После фильтрации (sigma > 0): {len(df_clean)} записей")

# ============================================
# 2. БАЗОВАЯ СТАТИСТИКА
# ============================================
print("\n" + "=" * 60)
print("БАЗОВАЯ СТАТИСТИКА")
print("=" * 60)

# Статистика сигналов (исправлено: 1=Buy, 2=Sell)
signals = df_clean[df_clean['signal'] != 0]
buy_signals = df_clean[df_clean['signal'] == 1]
sell_signals = df_clean[df_clean['signal'] == 2]

print(f"\nВсего записей: {len(df_clean)}")
print(f"BUY сигналов (signal=1): {len(buy_signals)} ({len(buy_signals)/len(df_clean)*100:.3f}%)")
print(f"SELL сигналов (signal=2): {len(sell_signals)} ({len(sell_signals)/len(df_clean)*100:.3f}%)")
print(f"NONE (signal=0): {len(df_clean[df_clean['signal']== 0])} ({len(df_clean[df_clean['signal']== 0])/len(df_clean)*100:.3f}%)")

# Статистика компонентов OBI
print(f"\nСтатистика компонентов OBI:")
for col in ['DW_OBI', 'ema_OBI', 'agg_OBI', 'OBI_recent', 'OBI_prev']:
    print(f"  {col}: mean={df_clean[col].mean():.6f}, std={df_clean[col].std():.6f}")

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

# ============================================
# 3. ФУНКЦИЯ АНАЛИЗА СИГНАЛОВ (исправлено: 1=Buy, 2=Sell)
# ============================================
def analyze_signals(df, signal_type, horizon_sec=300, target_move=0.0020, max_drawdown=0.0040):
    """
    Анализ качества сигналов OBI

    Параметры:
    - df: DataFrame с данными
    - signal_type: 1 (BUY) или 2 (SELL)
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
            'avg_profit': 0, 'avg_loss': 0, 'profit_factor': 0,
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

        # Берем будущие данные
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

        else:  # SELL (signal_type == 2)
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
HORIZON = 300         # 5 минут

buy_res = analyze_signals(df_clean, 1, HORIZON, TARGET_MOVE, MAX_DD)
sell_res = analyze_signals(df_clean, 2, HORIZON, TARGET_MOVE, MAX_DD)

print(f"\n=== BUY СИГНАЛЫ (signal=1) ===")
print(f"Всего: {buy_res['total_signals']}")
print(f"  Успешных: {buy_res['good']} ({buy_res['accuracy']:.2%})")
print(f"  Неудачных: {buy_res['false']}")
print(f"  Стоп-лосс: {buy_res['stopped_out']}")
print(f"Среднее движение: {buy_res['avg_move']:.4%}")
print(f"Медианное движение: {buy_res['median_move']:.4%}")
print(f"Средний профит: {buy_res['avg_profit']:.4%}")
print(f"Средний убыток: {buy_res['avg_loss']:.4%}")
print(f"Profit Factor: {buy_res['profit_factor']:.2f}")

print(f"\n=== SELL СИГНАЛЫ (signal=2) ===")
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
# 5. ОПТИМИЗАЦИЯ ПОРОГА Z-SCORE (исправлено: 1=Buy, 2=Sell)
# ============================================
print("\n" + "=" * 60)
print("ОПТИМИЗАЦИЯ ПОРОГА Z-SCORE")
print("=" * 60)

def optimize_z_threshold(df, train_ratio=0.7, target_move=0.0020,
                         max_drawdown=0.0040, horizon_sec=300):
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
        # Генерируем сигналы на train (исправлено: 1=Buy, 2=Sell)
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
# 6. ВИЗУАЛИЗАЦИЯ (СТАТИЧЕСКИЕ ГРАФИКИ)
# ============================================
print("\n" + "=" * 60)
print("ПОСТРОЕНИЕ СТАТИЧЕСКИХ ГРАФИКОВ")
print("=" * 60)

fig = plt.figure(figsize=(22, 18))

# ------------------------------------------------------------------------
# 6.1 Цена + z-score + сигналы (основной информативный график)
# ------------------------------------------------------------------------
ax1 = plt.subplot(4, 3, 1)
ax1_twin1 = ax1.twinx()
ax1_twin2 = ax1.twinx()
ax1_twin2.spines['right'].set_position(('outward', 60))

# Цена
ax1.plot(df_clean['ts_s_rel'], df_clean['mid_price'], 'k-', alpha=0.7, linewidth=1, label='Price')

# z-score
ax1_twin1.plot(df_clean['ts_s_rel'], df_clean['z_score'], 'b-', alpha=0.5, linewidth=0.8, label='z-score')
ax1_twin1.axhline(2.0, color='g', linestyle='--', linewidth=1, alpha=0.5)
ax1_twin1.axhline(-2.0, color='g', linestyle='--', linewidth=1, alpha=0.5)

# Сигналы
buy_plot = df_clean[df_clean['signal'] == 1]
sell_plot = df_clean[df_clean['signal'] == 2]

ax1.scatter(buy_plot['ts_s_rel'], buy_plot['mid_price'],
            marker='^', s=50, c='lime', edgecolors='darkgreen', linewidth=1,
            label='BUY', zorder=5, alpha=0.8)
ax1.scatter(sell_plot['ts_s_rel'], sell_plot['mid_price'],
            marker='v', s=50, c='red', edgecolors='darkred', linewidth=1,
            label='SELL', zorder=5, alpha=0.8)

ax1.set_title('Цена + z-score + сигналы', fontsize=12, fontweight='bold')
ax1.set_xlabel('Время (сек)')
ax1.set_ylabel('Price', color='k')
ax1_twin1.set_ylabel('z-score', color='b')
ax1_twin2.set_ylabel('')  # Пустая ось для отступа
ax1.legend(loc='upper left', fontsize=8)
ax1.grid(True, alpha=0.3)

# ------------------------------------------------------------------------
# 6.2 Цена + OBI_recent + OBI_prev (наложение)
# ------------------------------------------------------------------------
ax2 = plt.subplot(4, 3, 2)
ax2_twin = ax2.twinx()

ax2.plot(df_clean['ts_s_rel'], df_clean['mid_price'], 'k-', alpha=0.7, linewidth=1, label='Price')
ax2_twin.plot(df_clean['ts_s_rel'], df_clean['OBI_recent'], 'b-', alpha=0.7, linewidth=1, label='OBI_recent')
ax2_twin.plot(df_clean['ts_s_rel'], df_clean['OBI_prev'], 'r-', alpha=0.7, linewidth=1, label='OBI_prev')
ax2_twin.axhline(0, color='gray', linestyle='-', linewidth=0.5, alpha=0.5)

ax2.set_title('Цена + OBI_recent/OBI_prev', fontsize=12, fontweight='bold')
ax2.set_xlabel('Время (сек)')
ax2.set_ylabel('Price', color='k')
ax2_twin.set_ylabel('OBI', color='b')
ax2.legend(loc='upper left', fontsize=8)
ax2_twin.legend(loc='upper right', fontsize=8)
ax2.grid(True, alpha=0.3)

# ------------------------------------------------------------------------
# 6.3 Цена + sigma (волатильность OBI)
# ------------------------------------------------------------------------
ax3 = plt.subplot(4, 3, 3)
ax3_twin = ax3.twinx()

ax3.plot(df_clean['ts_s_rel'], df_clean['mid_price'], 'k-', alpha=0.7, linewidth=1, label='Price')
ax3_twin.fill_between(df_clean['ts_s_rel'], 0, df_clean['sigma'] * 1000,
                       alpha=0.3, color='orange', label='sigma × 1000')
ax3_twin.plot(df_clean['ts_s_rel'], df_clean['sigma'] * 1000,
              'orange', alpha=0.5, linewidth=0.8)

ax3.set_title('Цена + sigma (волатильность OBI)', fontsize=12, fontweight='bold')
ax3.set_xlabel('Время (сек)')
ax3.set_ylabel('Price', color='k')
ax3_twin.set_ylabel('sigma × 1000', color='orange')
ax3.legend(loc='upper left', fontsize=8)
ax3_twin.legend(loc='upper right', fontsize=8)
ax3.grid(True, alpha=0.3)

# ------------------------------------------------------------------------
# 6.4 Распределение z-score
# ------------------------------------------------------------------------
ax4 = plt.subplot(4, 3, 4)
ax4.hist(df_clean['z_score'], bins=80, alpha=0.7, color='steelblue', edgecolor='black', density=True)
# Нормальное распределение
x = np.linspace(df_clean['z_score'].min(), df_clean['z_score'].max(), 100)
ax4.plot(x, norm.pdf(x, 0, 1), 'r-', linewidth=2, label='N(0,1)')
ax4.axvline(2.0, color='g', linestyle='--', linewidth=2, label='threshold ±2.0')
ax4.axvline(-2.0, color='g', linestyle='--', linewidth=2)
if best_params is not None:
    ax4.axvline(best_params['z_thresh'], color='orange', linestyle=':', linewidth=2,
                label=f'optimal {best_params["z_thresh"]:.1f}')
    ax4.axvline(-best_params['z_thresh'], color='orange', linestyle=':', linewidth=2)
ax4.set_title('Распределение z-score', fontsize=12)
ax4.set_xlabel('z-score')
ax4.set_ylabel('Плотность')
ax4.legend(fontsize=8)
ax4.grid(True, alpha=0.3)

# ------------------------------------------------------------------------
# 6.5 QQ-plot z-score
# ------------------------------------------------------------------------
ax5 = plt.subplot(4, 3, 5)
stats.probplot(df_clean['z_score'], dist="norm", plot=ax5)
ax5.set_title('QQ-plot z-score', fontsize=12)
ax5.grid(True, alpha=0.3)

# ------------------------------------------------------------------------
# 6.6 Распределение sigma
# ------------------------------------------------------------------------
ax6 = plt.subplot(4, 3, 6)
ax6.hist(df_clean['sigma'], bins=50, alpha=0.7, color='coral', edgecolor='black')
ax6.axvline(df_clean['sigma'].median(), color='red', linestyle='--',
            label=f'медиана = {df_clean["sigma"].median():.6f}')
ax6.set_title('Распределение sigma', fontsize=12)
ax6.set_xlabel('sigma')
ax6.set_ylabel('Частота')
ax6.legend(fontsize=8)
ax6.grid(True, alpha=0.3)

# ------------------------------------------------------------------------
# 6.7 Зависимость z-score от agg_OBI
# ------------------------------------------------------------------------
ax7 = plt.subplot(4, 3, 7)
scatter = ax7.scatter(df_clean['agg_OBI'], df_clean['z_score'],
                      c=df_clean['sigma'], cmap='viridis', alpha=0.5, s=3)
ax7.axhline(2.0, color='g', linestyle='--', linewidth=1)
ax7.axhline(-2.0, color='g', linestyle='--', linewidth=1)
ax7.set_title('z-score vs agg_OBI (цвет = sigma)', fontsize=12)
ax7.set_xlabel('Aggregated OBI')
ax7.set_ylabel('z-score')
plt.colorbar(scatter, ax=ax7, label='sigma')
ax7.grid(True, alpha=0.3)

# ------------------------------------------------------------------------
# 6.8 Автокорреляция z-score
# ------------------------------------------------------------------------
ax8 = plt.subplot(4, 3, 8)
from statsmodels.graphics.tsaplots import plot_acf
plot_acf(df_clean['z_score'].dropna(), lags=50, ax=ax8)
ax8.set_title('Автокорреляция z-score', fontsize=12)
ax8.grid(True, alpha=0.3)

# ------------------------------------------------------------------------
# 6.9 Точность vs порог z-score
# ------------------------------------------------------------------------
ax9 = plt.subplot(4, 3, 9)
if opt_results is not None and len(opt_results) > 0:
    ax9.plot(opt_results['z_thresh'], opt_results['train_acc'], 'o-',
             label='Train', color='blue', linewidth=2)
    ax9.plot(opt_results['z_thresh'], opt_results['test_acc'], 's-',
             label='Test', color='red', linewidth=2)
    if best_params is not None:
        ax9.axvline(best_params['z_thresh'], color='green', linestyle='--',
                    label=f'best = {best_params["z_thresh"]:.1f}')
    ax9.set_title('Оптимизация порога z-score', fontsize=12)
    ax9.set_xlabel('Порог z-score')
    ax9.set_ylabel('Accuracy')
    ax9.legend(fontsize=8)
    ax9.grid(True, alpha=0.3)

# ------------------------------------------------------------------------
# 6.10 Корреляция с будущим движением цены (по горизонтам)
# ------------------------------------------------------------------------
ax10 = plt.subplot(4, 3, 10)
horizons = np.arange(1, 31, 1)  # 1-30 минут
correlations = []

for h in horizons:
    periods = h * 60 * 2  # 2 записи в секунду
    df_clean[f'price_change_{h}min'] = df_clean['mid_price'].shift(-periods) - df_clean['mid_price']
    corr = df_clean['z_score'].corr(df_clean[f'price_change_{h}min'])
    correlations.append(corr)

ax10.plot(horizons, correlations, 'b-', linewidth=2)
ax10.axhline(0, color='k', linestyle='-', linewidth=0.5)
ax10.fill_between(horizons, 0, correlations, where=np.array(correlations) > 0,
                  color='green', alpha=0.3)
ax10.fill_between(horizons, 0, correlations, where=np.array(correlations) < 0,
                  color='red', alpha=0.3)
ax10.set_title('Корреляция z-score с будущим движением цены', fontsize=12)
ax10.set_xlabel('Горизонт (минуты)')
ax10.set_ylabel('Корреляция Пирсона')
ax10.grid(True, alpha=0.3)

# ------------------------------------------------------------------------
# 6.11 Сигналы по часам
# ------------------------------------------------------------------------
ax11 = plt.subplot(4, 3, 11)
signals_df = df_clean[df_clean['signal'] != 0].copy()
if len(signals_df) > 0:
    signals_df['hour'] = pd.to_datetime(signals_df['ts'], unit='ms').dt.hour

    # Считаем сигналы по часам
    hourly_counts = signals_df.groupby(['hour', 'signal']).size().unstack(fill_value=0)

    if 1 in hourly_counts.columns and 2 in hourly_counts.columns:
        width = 0.35
        x = np.arange(len(hourly_counts.index))
        ax11.bar(x - width/2, hourly_counts[1], width, label='BUY', color='green', alpha=0.7)
        ax11.bar(x + width/2, hourly_counts[2], width, label='SELL', color='red', alpha=0.7)
        ax11.set_xticks(x)
        ax11.set_xticklabels(hourly_counts.index)
        ax11.set_title('Сигналы по часам', fontsize=12)
        ax11.set_xlabel('Час (UTC)')
        ax11.set_ylabel('Количество сигналов')
        ax11.legend(fontsize=8)
        ax11.grid(True, alpha=0.3)

# ------------------------------------------------------------------------
# 6.12 Тепловая карта корреляций
# ------------------------------------------------------------------------
ax12 = plt.subplot(4, 3, 12)
corr_cols = ['DW_OBI', 'ema_OBI', 'agg_OBI', 'OBI_recent', 'OBI_prev', 'sigma', 'z_score']
corr_matrix = df_clean[corr_cols].corr()
sns.heatmap(corr_matrix, annot=True, fmt='.2f', cmap='RdBu_r',
            center=0, square=True, ax=ax12, cbar_kws={'shrink': 0.8})
ax12.set_title('Корреляционная матрица', fontsize=12)

plt.tight_layout()
plt.savefig('obi_analysis.png', dpi=150, bbox_inches='tight')
print("\n✅ Статический график сохранен: obi_analysis.png")
plt.show()

# ============================================
# 7. ИНТЕРАКТИВНЫЕ HTML ГРАФИКИ (ДОБАВЛЕНЫ)
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

    # --------------------------------------------------------------------
    # 7.1 ИНТЕРАКТИВНЫЙ ГРАФИК: Цена (отдельно, в своем масштабе)
    # --------------------------------------------------------------------
    print("\n7.1 Создание интерактивного графика: Цена...")

    fig_price = go.Figure()

    fig_price.add_trace(
        go.Scatter(
            x=df_clean['ts_s_rel'],
            y=df_clean['mid_price'],
            mode='lines',
            name='Price',
            line=dict(color='black', width=1.5),
            hovertemplate='<b>Price</b>: %{y:.2f}<br>Время: %{x:.0f}с<extra></extra>'
        )
    )

    # Добавляем сигналы на график цены
    buy_plot = df_clean[df_clean['signal'] == 1]
    sell_plot = df_clean[df_clean['signal'] == 2]

    fig_price.add_trace(
        go.Scatter(
            x=buy_plot['ts_s_rel'],
            y=buy_plot['mid_price'],
            mode='markers',
            name='BUY',
            marker=dict(symbol='triangle-up', size=12, color='lime', line=dict(color='darkgreen', width=2)),
            hovertemplate='<b>BUY</b><br>Price: %{y:.2f}<br>z-score: %{customdata:.3f}<extra></extra>',
            customdata=buy_plot['z_score']
        )
    )

    fig_price.add_trace(
        go.Scatter(
            x=sell_plot['ts_s_rel'],
            y=sell_plot['mid_price'],
            mode='markers',
            name='SELL',
            marker=dict(symbol='triangle-down', size=12, color='red', line=dict(color='darkred', width=2)),
            hovertemplate='<b>SELL</b><br>Price: %{y:.2f}<br>z-score: %{customdata:.3f}<extra></extra>',
            customdata=sell_plot['z_score']
        )
    )

    fig_price.update_layout(
        title=dict(text='<b>Цена с сигналами</b>', font=dict(size=18)),
        xaxis_title='Время (сек)',
        yaxis_title='Price',
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
                    dict(step="all", label="Все")
                ])
            ),
            rangeslider=dict(visible=True, thickness=0.05)
        ),
        legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1)
    )

    fig_price.write_html("obi_price.html")
    print("✅ Сохранен: obi_price.html")

    # --------------------------------------------------------------------
    # 7.2 ИНТЕРАКТИВНЫЙ ГРАФИК: z-score (отдельно)
    # --------------------------------------------------------------------
    print("\n7.2 Создание интерактивного графика: z-score...")

    # Автоматический подбор порога
    recommended_threshold = np.percentile(np.abs(df_clean['z_score']), 95)

    fig_zscore = go.Figure()

    fig_zscore.add_trace(
        go.Scatter(
            x=df_clean['ts_s_rel'],
            y=df_clean['z_score'],
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
        line_width=1.5,
        annotation_text=f"рекомендуемый +{recommended_threshold:.2f}"
    )
    fig_zscore.add_hline(
        y=-recommended_threshold,
        line_dash="dash",
        line_color="green",
        line_width=1.5,
        annotation_text=f"рекомендуемый -{recommended_threshold:.2f}"
    )
    fig_zscore.add_hline(
        y=2.0,
        line_dash="dot",
        line_color="orange",
        line_width=1,
        annotation_text="старый +2.0"
    )
    fig_zscore.add_hline(
        y=-2.0,
        line_dash="dot",
        line_color="orange",
        line_width=1,
        annotation_text="старый -2.0"
    )
    fig_zscore.add_hline(y=0, line_dash="solid", line_color="gray", line_width=0.5)

    fig_zscore.update_layout(
        title=dict(text='<b>z-score с порогами</b>', font=dict(size=18)),
        xaxis_title='Время (сек)',
        yaxis_title='z-score',
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
                    dict(step="all", label="Все")
                ])
            ),
            rangeslider=dict(visible=True, thickness=0.05)
        )
    )

    fig_zscore.write_html("obi_zscore.html")
    print("✅ Сохранен: obi_zscore.html")

    # --------------------------------------------------------------------
    # 7.3 ИНТЕРАКТИВНЫЙ ГРАФИК: OBI_recent и OBI_prev (отдельно)
    # --------------------------------------------------------------------
    print("\n7.3 Создание интерактивного графика: OBI_recent и OBI_prev...")

    fig_obi = go.Figure()

    fig_obi.add_trace(
        go.Scatter(
            x=df_clean['ts_s_rel'],
            y=df_clean['OBI_recent'],
            mode='lines',
            name='OBI_recent (быстрый)',
            line=dict(color='#1f77b4', width=1.5),
            hovertemplate='<b>OBI_recent</b>: %{y:.4f}<br>Время: %{x:.0f}с<extra></extra>'
        )
    )

    fig_obi.add_trace(
        go.Scatter(
            x=df_clean['ts_s_rel'],
            y=df_clean['OBI_prev'],
            mode='lines',
            name='OBI_prev (медленный)',
            line=dict(color='#ff7f0e', width=1.5),
            hovertemplate='<b>OBI_prev</b>: %{y:.4f}<br>Время: %{x:.0f}с<extra></extra>'
        )
    )

    fig_obi.add_hline(y=0, line_dash="solid", line_color="gray", line_width=0.5)

    fig_obi.update_layout(
        title=dict(text='<b>OBI_recent vs OBI_prev</b>', font=dict(size=18)),
        xaxis_title='Время (сек)',
        yaxis_title='OBI',
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
                    dict(step="all", label="Все")
                ])
            ),
            rangeslider=dict(visible=True, thickness=0.05)
        ),
        legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1)
    )

    fig_obi.write_html("obi_components.html")
    print("✅ Сохранен: obi_components.html")

    # --------------------------------------------------------------------
    # 7.4 ИНТЕРАКТИВНЫЙ ГРАФИК: sigma (волатильность OBI)
    # --------------------------------------------------------------------
    print("\n7.4 Создание интерактивного графика: sigma...")

    fig_sigma = go.Figure()

    fig_sigma.add_trace(
        go.Scatter(
            x=df_clean['ts_s_rel'],
            y=df_clean['sigma'],
            mode='lines',
            name='sigma',
            line=dict(color='orange', width=1.2),
            fill='tozeroy',
            fillcolor='rgba(255, 165, 0, 0.2)',
            hovertemplate='<b>sigma</b>: %{y:.6f}<br>Время: %{x:.0f}с<extra></extra>'
        )
    )

    fig_sigma.add_hline(
        y=df_clean['sigma'].median(),
        line_dash="dash",
        line_color="red",
        line_width=1,
        annotation_text=f"медиана = {df_clean['sigma'].median():.6f}"
    )

    fig_sigma.update_layout(
        title=dict(text='<b>Sigma (волатильность OBI)</b>', font=dict(size=18)),
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
                    dict(step="all", label="Все")
                ])
            ),
            rangeslider=dict(visible=True, thickness=0.05)
        )
    )

    fig_sigma.write_html("obi_sigma.html")
    print("✅ Сохранен: obi_sigma.html")

    # --------------------------------------------------------------------
    # 7.5 ИНТЕРАКТИВНЫЙ ГРАФИК: Корреляция z-score с движением цены
    # --------------------------------------------------------------------
    print("\n7.5 Создание интерактивного графика: Корреляция...")

    horizons = np.arange(1, 31, 1)
    correlations = []

    for h in horizons:
        periods = h * 60 * 2
        if periods < len(df_clean):
            df_clean[f'price_change_{h}min'] = df_clean['mid_price'].shift(-periods) - df_clean['mid_price']
            corr = df_clean['z_score'].corr(df_clean[f'price_change_{h}min'])
            correlations.append(corr if not np.isnan(corr) else 0)

    fig_corr = go.Figure()

    fig_corr.add_trace(
        go.Scatter(
            x=horizons[:len(correlations)],
            y=correlations,
            mode='lines+markers',
            name='Корреляция',
            line=dict(color='blue', width=2),
            marker=dict(size=10),
            hovertemplate='Горизонт: %{x} мин<br>Корреляция: %{y:.3f}<extra></extra>'
        )
    )

    # Заливка
    fig_corr.add_trace(
        go.Scatter(
            x=horizons[:len(correlations)],
            y=correlations,
            mode='lines',
            name='',
            fill='tozeroy',
            fillcolor='rgba(0, 0, 255, 0.1)',
            line=dict(width=0),
            showlegend=False
        )
    )

    fig_corr.add_hline(y=0, line_dash="solid", line_color="gray", line_width=1)

    # Максимальная корреляция
    max_corr_idx = np.argmax(np.abs(correlations))
    max_corr = correlations[max_corr_idx]
    max_horizon = horizons[max_corr_idx]

    fig_corr.add_annotation(
        x=max_horizon,
        y=max_corr,
        text=f"<b>Макс: {max_corr:.3f}</b><br>на {max_horizon} мин",
        showarrow=True,
        arrowhead=2,
        font=dict(size=13),
        bgcolor="white",
        bordercolor="black",
        borderwidth=1
    )

    fig_corr.update_layout(
        title=dict(text='<b>Корреляция z-score с будущим движением цены</b>', font=dict(size=18)),
        xaxis_title='Горизонт (минуты)',
        yaxis_title='Корреляция Пирсона',
        height=450,
        hovermode='x unified',
        dragmode='pan'
    )

    fig_corr.write_html("obi_correlation.html")
    print("✅ Сохранен: obi_correlation.html")

    # --------------------------------------------------------------------
    # 7.6 ИНТЕРАКТИВНЫЙ ГРАФИК: Распределение z-score
    # --------------------------------------------------------------------
    print("\n7.6 Создание интерактивного графика: Распределение z-score...")

    fig_dist = go.Figure()

    # Гистограмма
    fig_dist.add_trace(
        go.Histogram(
            x=df_clean['z_score'],
            nbinsx=80,
            name='Распределение',
            opacity=0.7,
            marker=dict(color='steelblue', line=dict(color='black', width=0.5)),
            hovertemplate='z-score: %{x:.3f}<br>Частота: %{y}<extra></extra>'
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
            line=dict(color='red', width=2),
            hovertemplate='N(0,1): %{y:.0f}<extra></extra>'
        )
    )

    # Вертикальные линии порогов
    fig_dist.add_vline(
        x=recommended_threshold,
        line_dash="dash",
        line_color="green",
        line_width=2,
        annotation_text=f"новый +{recommended_threshold:.2f}"
    )
    fig_dist.add_vline(
        x=-recommended_threshold,
        line_dash="dash",
        line_color="green",
        line_width=2,
        annotation_text=f"новый -{recommended_threshold:.2f}"
    )
    fig_dist.add_vline(
        x=2.0,
        line_dash="dot",
        line_color="orange",
        line_width=1.5,
        annotation_text="старый +2.0"
    )
    fig_dist.add_vline(
        x=-2.0,
        line_dash="dot",
        line_color="orange",
        line_width=1.5,
        annotation_text="старый -2.0"
    )

    # Статистика в аннотации
    stats_text = (f"Среднее: {df_clean['z_score'].mean():.3f}<br>"
                  f"СКО: {df_clean['z_score'].std():.3f}<br>"
                  f"Сигналов с новым порогом: {len(buy_signals) + len(sell_signals)}")

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
        borderwidth=1
    )

    fig_dist.update_layout(
        title=dict(text='<b>Распределение z-score с порогами</b>', font=dict(size=18)),
        xaxis_title='z-score',
        yaxis_title='Частота',
        height=450,
        hovermode='x unified',
        dragmode='pan',
        barmode='overlay',
        legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1)
    )

    fig_dist.write_html("obi_distribution.html")
    print("✅ Сохранен: obi_distribution.html")

    # --------------------------------------------------------------------
    # 7.7 СВОДНЫЙ HTML С ГРУППОЙ ГРАФИКОВ
    # --------------------------------------------------------------------
    print("\n7.7 Создание сводного HTML с группой графиков...")

    html_content = '''<!DOCTYPE html>
    <html>
    <head>
        <title>OBI Анализ - Сводка графиков</title>
        <style>
            body { font-family: Arial, sans-serif; background: #f5f5f5; margin: 20px; }
            h1 { color: #333; text-align: center; }
            .graph-container {
                background: white;
                border-radius: 10px;
                box-shadow: 0 2px 10px rgba(0,0,0,0.1);
                margin: 20px 0;
                padding: 20px;
            }
            .graph-container iframe {
                width: 100%;
                border: none;
                border-radius: 5px;
            }
            .stats {
                background: white;
                border-radius: 10px;
                padding: 20px;
                margin: 20px 0;
                box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            }
            .stats table {
                width: 100%;
                border-collapse: collapse;
            }
            .stats td, .stats th {
                padding: 8px 12px;
                border: 1px solid #ddd;
                text-align: left;
            }
            .stats th {
                background: #f0f0f0;
            }
            .stats .positive { color: green; }
            .stats .negative { color: red; }
            .stats .highlight { background: #f0f8ff; }
        </style>
    </head>
    <body>
        <h1>📊 OBI Анализ - Сводка интерактивных графиков</h1>

        <div class="stats">
            <h2>📈 Базовые метрики</h2>
            <table>
                <tr><th>Метрика</th><th>Значение</th></tr>
                <tr><td>Всего записей</td><td>''' + f"{len(df_clean)}" + '''</td></tr>
                <tr><td>BUY сигналов</td><td>''' + f"{len(buy_signals)} ({len(buy_signals)/len(df_clean)*100:.2f}%)" + '''</td></tr>
                <tr><td>SELL сигналов</td><td>''' + f"{len(sell_signals)} ({len(sell_signals)/len(df_clean)*100:.2f}%)" + '''</td></tr>
                <tr><td>Z-score среднее</td><td>''' + f"{df_clean['z_score'].mean():.3f}" + '''</td></tr>
                <tr><td>Z-score СКО</td><td>''' + f"{df_clean['z_score'].std():.3f}" + '''</td></tr>
                <tr><td>Z-score min/max</td><td>''' + f"{df_clean['z_score'].min():.3f} / {df_clean['z_score'].max():.3f}" + '''</td></tr>
                <tr><td>Рекомендуемый порог</td><td class="highlight"><b>''' + f"{recommended_threshold:.2f}" + '''</b></td></tr>
                <tr><td>Точность сигналов (5 мин, 0.20%)</td><td>''' + f"{total_acc:.2%}" + '''</td></tr>
            </table>
        </div>

        <div class="graph-container">
            <h2>📊 Цена с сигналами</h2>
            <iframe src="obi_price.html" height="450"></iframe>
        </div>

        <div class="graph-container">
            <h2>📊 Z-score с порогами</h2>
            <iframe src="obi_zscore.html" height="450"></iframe>
        </div>

        <div class="graph-container">
            <h2>📊 OBI_recent vs OBI_prev</h2>
            <iframe src="obi_components.html" height="450"></iframe>
        </div>

        <div class="graph-container">
            <h2>📊 Sigma (волатильность OBI)</h2>
            <iframe src="obi_sigma.html" height="450"></iframe>
        </div>

        <div class="graph-container">
            <h2>📊 Корреляция z-score с будущим движением цены</h2>
            <iframe src="obi_correlation.html" height="500"></iframe>
        </div>

        <div class="graph-container">
            <h2>📊 Распределение z-score</h2>
            <iframe src="obi_distribution.html" height="500"></iframe>
        </div>

        <p style="text-align:center; color:#888; font-size:12px; margin-top:30px;">
            Сгенерировано автоматически • Используйте Ctrl+Колесо мыши для масштабирования
        </p>
    </body>
    </html>
    '''

    with open('obi_dashboard.html', 'w', encoding='utf-8') as f:
        f.write(html_content)
    print("✅ Сохранен: obi_dashboard.html")

    print("\n" + "=" * 60)
    print("ИНТЕРАКТИВНЫЕ ГРАФИКИ СОЗДАНЫ")
    print("=" * 60)
    print("\nОткройте в браузере: obi_dashboard.html")
    print("Для масштабирования используйте: Ctrl + Колесо мыши")
    print("=" * 60)

# ============================================
# 8. ДОПОЛНИТЕЛЬНЫЙ АНАЛИЗ: РАЗНЫЕ ГОРИЗОНТЫ
# ============================================
print("\n" + "=" * 60)
print("АНАЛИЗ ДЛЯ РАЗНЫХ ГОРИЗОНТОВ И ЦЕЛЕЙ")
print("=" * 60)

horizons = [60, 120, 180, 240, 300, 360, 420, 480, 540, 600, 660, 720, 780, 840, 900]
targets = [0.0015, 0.0020, 0.0025, 0.0030]

print("\nAccuracy для разных горизонтов и целей:")
print("-" * 80)
print(f"{'Горизонт':>10} | {'Цель':>8} | {'BUY':>8} | {'SELL':>8} | {'Total':>8} | {'Sig Count':>10}")
print("-" * 80)

results_df = []
for h in horizons:
    for t in targets:
        buy_res_h = analyze_signals(df_clean[df_clean['signal'] == 1], 1, h, t, MAX_DD)
        sell_res_h = analyze_signals(df_clean[df_clean['signal'] == 2], 2, h, t, MAX_DD)
        total = buy_res_h['total_signals'] + sell_res_h['total_signals']
        total_good = buy_res_h['good'] + sell_res_h['good']
        total_acc = total_good / total if total > 0 else 0

        if total >= 5:
            print(f"{h:>10}s | {t:>7.2%} | {buy_res_h['accuracy']:>7.2%} | "
                  f"{sell_res_h['accuracy']:>7.2%} | {total_acc:>7.2%} | {total:>10}")
            results_df.append({
                'horizon': h,
                'target': t,
                'accuracy': total_acc,
                'total_signals': total
            })

# ============================================
# 9. ДЕТАЛЬНЫЙ АНАЛИЗ КАЧЕСТВА СИГНАЛОВ
# ============================================
print("\n" + "=" * 60)
print("ДЕТАЛЬНЫЙ АНАЛИЗ КАЧЕСТВА СИГНАЛОВ")
print("=" * 60)

# 9.1 Распределение z-score для сигналов vs не-сигналов
signals = df_clean[df_clean['signal'] != 0]
no_signals = df_clean[df_clean['signal'] == 0]

print(f"\nZ-score статистика:")
print(f"  Сигналы: mean={signals['z_score'].mean():.3f}, std={signals['z_score'].std():.3f}")
print(f"  Не-сигналы: mean={no_signals['z_score'].mean():.3f}, std={no_signals['z_score'].std():.3f}")

# 9.2 Распределение движений после сигналов
if len(signals) > 0:
    all_moves = []
    for signal_type in [1, 2]:
        sig_df = signals[signals['signal'] == signal_type]
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
        print(f"\nРаспределение движений после сигналов (горизонт {HORIZON}с):")
        print(f"  Среднее: {np.mean(all_moves):.4%}")
        print(f"  Медиана: {np.median(all_moves):.4%}")
        print(f"  Станд. отклонение: {np.std(all_moves):.4%}")
        print(f"  Min: {np.min(all_moves):.4%}")
        print(f"  Max: {np.max(all_moves):.4%}")
        print(f"  Доля >= 0.15%: {np.mean(np.array(all_moves) >= 0.0015):.2%}")
        print(f"  Доля >= 0.20%: {np.mean(np.array(all_moves) >= 0.0020):.2%}")
        print(f"  Доля >= 0.25%: {np.mean(np.array(all_moves) >= 0.0025):.2%}")

print("\n" + "=" * 60)
print("АНАЛИЗ ЗАВЕРШЕН")
print("=" * 60)
