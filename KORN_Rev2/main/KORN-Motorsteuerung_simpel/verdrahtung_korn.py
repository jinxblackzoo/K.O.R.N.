# filename: verdrahtung_korn.py
# pip: matplotlib

import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyArrowPatch

# Konfiguration (bei Bedarf anpassen)
PINS = {
    "PUL_PIN": 2,
    "DIR_PIN": 3,
    "ENA_PIN": 5,
    "OPTO_PIN": 4,  # Reserve laut Sketch
    "BUZZER_PIN": 6,
    "RELAY_PIN": 8,
    "I2C_SDA": "A4",
    "I2C_SCL": "A5",
}
NOTES = [
    "DS3231: VCC→3.3V/5V, GND→GND, SDA→A4, SCL→A5, SQW→Pin 2 (optional!)",
    f"Motortreiber: PUL+→Pin {PINS['PUL_PIN']}, DIR+→Pin {PINS['DIR_PIN']}, ENA+→Pin {PINS['ENA_PIN']}",
    f"Relais: IN→Pin {PINS['RELAY_PIN']}",
    f"Buzzer (aktiv): SIG→Pin {PINS['BUZZER_PIN']}, VCC→5V (oder VIN), GND→GND",
    "Hinweis: Pin 2 ist für PUL+ belegt; DS3231 SQW nur optional oder auf anderen freien Interrupt-Pin legen.",
]

# Layout-Koordinaten
# Wir legen einfache Boxen an und verbinden sie mit Pfeilen
nodes = {
    "Arduino": (0.5, 0.7, 0.22, 0.2),
    "DS3231": (0.12, 0.72, 0.18, 0.16),
    "Driver":  (0.82, 0.72, 0.18, 0.16),
    "Relay":   (0.82, 0.48, 0.18, 0.16),
    "Buzzer":  (0.12, 0.48, 0.18, 0.16),
    "Power":   (0.5, 0.18, 0.22, 0.16),
}

def add_box(ax, label, xywh, fc="#f5f5f5"):
    (x, y, w, h) = xywh
    rect = Rectangle((x, y), w, h, edgecolor="#333", facecolor=fc, linewidth=1.5)
    ax.add_patch(rect)
    ax.text(x + w/2, y + h/2, label, ha="center", va="center", fontsize=10, weight="bold")
    return rect

def connect(ax, src_xy, dst_xy, text=None, color="#333"):
    arrow = FancyArrowPatch(src_xy, dst_xy, arrowstyle="-|>", mutation_scale=10, lw=1.4, color=color)
    ax.add_patch(arrow)
    if text:
        # Text in die Mitte der Verbindung
        mid = ((src_xy[0] + dst_xy[0]) / 2, (src_xy[1] + dst_xy[1]) / 2)
        ax.text(mid[0], mid[1], text, fontsize=8, color=color, ha="center", va="bottom")

def center_of(rect, side):
    x, y = rect.get_xy()
    w, h = rect.get_width(), rect.get_height()
    if side == "left":   return (x, y + h/2)
    if side == "right":  return (x + w, y + h/2)
    if side == "top":    return (x + w/2, y + h)
    if side == "bottom": return (x + w/2, y)
    return (x + w/2, y + h/2)

def main():
    fig, ax = plt.subplots(figsize=(10, 7))
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")

    # Boxen
    ar = add_box(ax, "Arduino\n(UNO/Nano)", nodes["Arduino"])
    ds = add_box(ax, "DS3231\nRTC", nodes["DS3231"])
    dr = add_box(ax, "Motortreiber", nodes["Driver"])
    rl = add_box(ax, "Relais\n(Treiber-Power)", nodes["Relay"])
    bz = add_box(ax, "Buzzer (aktiv)", nodes["Buzzer"])
    pw = add_box(ax, "Power\n(5V / GND / VIN)", nodes["Power"])

    # Verbindungen DS3231 ↔ Arduino (I2C)
    connect(ax, center_of(ds, "right"), center_of(ar, "left"),
            text=f"SDA→{PINS['I2C_SDA']}, SCL→{PINS['I2C_SCL']}", color="#1f77b4")
    connect(ax, center_of(pw, "top"), center_of(ds, "bottom"), text="VCC, GND", color="#1f77b4")
    # SQW optional (Hinweistext als gestrichelt)
    connect(ax, (center_of(ds, "right")[0]+0.01, center_of(ds, "right")[1]-0.06),
            (center_of(ar, "left")[0]-0.01, center_of(ar, "left")[1]-0.1),
            text=f"SQW→Pin 2 (optional!)", color="#1f77b4")

    # Arduino → Motortreiber
    connect(ax, center_of(ar, "right"), center_of(dr, "left"),
            text=f"PUL+→{PINS['PUL_PIN']}, DIR+→{PINS['DIR_PIN']}, ENA+→{PINS['ENA_PIN']}", color="#2ca02c")

    # Arduino → Relais (IN), Power → Relais (VCC/GND)
    connect(ax, (center_of(ar, "right")[0], center_of(ar, "right")[1]-0.12), center_of(rl, "left"),
            text=f"IN→{PINS['RELAY_PIN']}", color="#ff7f0e")
    connect(ax, center_of(pw, "top"), center_of(rl, "bottom"), text="VCC, GND", color="#ff7f0e")

    # Arduino → Buzzer (SIG), Power → Buzzer (VCC/GND)
    connect(ax, center_of(ar, "left"), center_of(bz, "right"),
            text=f"SIG→{PINS['BUZZER_PIN']}", color="#d62728")
    connect(ax, center_of(pw, "top"), center_of(bz, "bottom"),
            text="VCC (5V/VIN), GND", color="#d62728")

    # Gemeinsame Masse
    ax.text(0.5, 0.08, "Gemeinsame Masse (GND) zwischen Arduino, DS3231, Treiber, Relais, Buzzer", 
            ha="center", va="center", fontsize=9, color="#555")

    # Notizen
    y_note = 0.03
    for note in NOTES:
        ax.text(0.02, y_note, f"• {note}", fontsize=8, color="#333", ha="left", va="center")
        y_note += 0.03

    plt.tight_layout()
    plt.savefig("verdrahtung.svg", bbox_inches="tight")
    plt.savefig("verdrahtung.png", dpi=200, bbox_inches="tight")
    print("Erstellt: verdrahtung.svg und verdrahtung.png")

if __name__ == "__main__":
    main()