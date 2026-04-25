# Enclosure & 3D Printing Documentation

This document covers the 3D-printed enclosures, grommets, and gasket used in the XIAO/SCD41 sensor units. CAD files are available on Onshape (public) and exported STEP and STL files are provided in the `CAD` folder of this repository.

---

## Table of Contents

- [Onshape Documents](#onshape-documents)
- [Bill of Materials](#bill-of-materials)
- [Filament & Print Settings](#filament--print-settings)
- [Parts Overview](#parts-overview)
  - [XIAO Control Enclosure](#xiao-control-enclosure)
  - [SCD41 Sensor Enclosure](#scd41-sensor-enclosure)
  - [Cable Grommet & Gasket](#cable-grommet--gasket)
- [Hardware & Assembly Notes](#hardware--assembly-notes)

---

## Onshape Documents

These documents are public and accessible to all Onshape users.
**Owned by:** BillJuv

| Document | Link |
|---|---|
| XIAO/SCD41/MAX17048 PCB | [Open in Onshape](https://cad.onshape.com/documents/322b31f4cb789d0ca7d721ed/w/ee8c4b56b1603b4e20ae5080/e/40f79fbb32535b3e9f26fa67) |
| SCD41_XIAO Enclosure v4.0 | [Open in Onshape](https://cad.onshape.com/documents/fcc89ce80f74fb06b3c73a24/w/bfc54d0a06e95b34f7452eb1/e/d634f8e0859f714640674855) |
| Cable Grommet | [Open in Onshape](https://cad.onshape.com/documents/550715f3ed90ea8fc4acac6c/w/30db51a251d38ff25dbee160/e/a918008760f5b729e8163560) |

---

## Bill of Materials

| Component | Details | Source |
|---|---|---|
| PETG Filament | Overture PETG | [Amazon](https://www.amazon.com/OVERTURE-Filament-Consumables-Dimensional-Accuracy/dp/B07PDV9RC8/ref=sr_1_1_pp?dib=eyJ2IjoiMSJ9.z4MlxnKNtLOHoVnvh2oP8H0K8s-CFVDJ4v982m4bLQODpiXLCpETpcVSR9BAd5oXkeuO7ngcwz_oPgcJKuHpii5Vgs4UHbpOsM5J0sIyd8iHP2K-NKucFK9leWvfs4betKIu6tRBtn5Ulej7RkpNpHbrlm7kwLzL9K3kecc7cVA8pvCW7EoHUi4KExtqC62Rh1Rw9CYEdPRO55hypme0CgqlgAhBViyAbSfWksgZUHY.O4-dCwqv886fiZ_hDR48X6P3_b85QcAK0loUv2RN2Y4&dib_tag=se&keywords=OVERTURE%2Bpetg&qid=1777092897&sbo=RZvfv%2F%2FHxDF%2BO5021pAnSA%3D%3D&sr=8-1&th=1) |
| TPU Filament | Overture "High Speed" TPU (95A)| [Amazon](https://www.amazon.com/dp/B08BC654PS?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1) |
| Brass Threaded Inserts (2.5mm) | For lid and cap mounting | [Amazon](https://www.amazon.com/dp/B0FJXLGHY4?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_1) |
| Screws (2mm x 5mm) | PCB mounting — 2 per unit | [Amazon](https://www.amazon.com/dp/B082XXXRV6?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_3)|
| Screws (2.5mm x 5mm) | Lid and cap mounting | [Amazon](https://www.amazon.com/dp/B07YS94FFP?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_5) |

Other parts referenced are found in the Bill of Materials on the main ReadMe
---

## Filament & Print Settings

3D printing was done on a Bambu Labs A1 printer using Overture PETG filament for the enclosures and Overture 95A TPU for the grommets and gasket. Bambu slicer profiles used were "Generic PETG" for the PETG and "Bambu TPU 95A HF" for the TPU. Part orientation was set using the Bambu slicer Auto-Orient feature. No supports are required for any part.

> **Note:** TPU cannot be loaded through the AMS — it must be fed directly. Filaments softer than 95A may not feed reliably with this printer.

PETG was chosen for its humidity resistance. TPU was chosen for the grommets and gasket for flexibility and a good seal.

---

## Parts Overview

### XIAO Control Enclosure

<img src=Attachments/Photos/XIAO_control_enclosure.jpg width="70%"/>
<img src=Attachments/Photos/XiAO_Top.jpeg width="70%"/>

This enclosure houses the XIAO ESP32S3 + Wio-SX1262, MAX17048 fuel gauge, and LiPo battery (mounted underneath the PCB). It is designed for humidity resistance, using an IP65-rated on/off switch and a TPU cable grommet for the sensor cable entry. Additional sealing of the antenna connector and USB-C port can be achieved with conformal coating or silicone sealant if needed.

**Design details:**
- The PCB mounting posts are threaded in CAD for 2mm fine-thread screws (2 screws are sufficient)
- The lid is 3mm thick for stiffness and to apply adequate compression to the TPU gasket
- Lid is secured with 2.5mm fine-thread screws into brass threaded inserts
- A mounting tab on the side allows the box to be hung or surface-mounted

> **Note:** "Threaded in CAD" means the holes are modeled with threads — no tap tool is required. Just drive the screw directly into the printed hole.

---

### SCD41 Sensor Enclosure

<img src=Attachments/Photos/SCD4x_box_cap.jpg width="70%"/>
<img src=Attachments/Photos/SCD4X_box.jpg width="70%"/>
<img src=Attachments/Photos/SCD_Enc1.jpeg width="70%"/>
<img src=Attachments/Photos/Print_orientation.jpg width="70%"/>

This enclosure is sized to fit the Adafruit SCD41 breakout board. The cap design allows the cable to enter from the top, so the unit can be hung with the sensor facing downward for optimal airflow per the Sensirion datasheet recommendations.

**Design details:**
- Cap face screws are 2.5mm fine-thread into brass threaded inserts in the cap
- PCB post holes are threaded in CAD for 2.5mm fine-thread screws
- Self-adhesive PTFE filter membranes (20mm round) are applied over the vent holes on the bottom and two sides
- A smooth print plate is recommended — clean the surfaces with isopropyl alcohol before applying PTFE membranes
- Replace PTFE membranes as needed

---

### Cable Grommet & Gasket

<img src=Attachments/Photos/cable_grommet.jpg width="70%"/>

The cable grommet is sized for a 4–5mm diameter cable and is used in both the XIAO control box and the SCD41 sensor cap. The lid gasket is a separate CAD part within the enclosure assembly. Both are printed in TPU for flexibility and a weather-resistant seal.

---

## Hardware & Assembly Notes

- **Brass inserts** are installed using a soldering iron with a flat or insert tip. Set the iron to around 200–220°C and press slowly and evenly
- **Battery polarity** — LiPo batteries frequently have reversed polarity on JST connectors. Verify polarity before connecting. The JST 2.0 connector pins can be swapped easily with a pin tool
- **PTFE membranes** — clean mounting surfaces thoroughly with isopropyl alcohol before applying for best adhesion. Press firmly and allow to set before exposing to humidity
- **Assembly order** — install brass inserts before any wiring; it is easier to work with the enclosure empty

---

*Documentation by BillJuv — part of the Nevada mushroom grow monitoring project.*
*Public project documentation: [billjuv.github.io](https://billjuv.github.io)*