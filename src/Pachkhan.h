#ifndef PACHKHAN_H
#define PACHKHAN_H

struct PachkhanEntry {
  const char* name;
  const char* description;
  const char* transcript;
};

// Array of 16 Pachkhan practices
static const PachkhanEntry pachkhanData[] = {
  {
    "Navkarshi Pachchakhan",
    "Navkar 48 min after sunrise",
    "Sit in meditative posture with closed fist. Recite Navkar 3x. Then eat or drink."
  },
  {
    "Porsi Pachchakhan",
    "Navkar 1 Prahar (3 hours) after sunrise",
    "Sit in meditative posture with closed fist. Recite Navkar 3x. Then eat or drink."
  },
  {
    "Sadhporishi Pachchakhan",
    "Navkar 1.5 Prahar (4.5 hours) after sunrise",
    "Sit in meditative posture with closed fist. Recite Navkar 3x. Then eat or drink."
  },
  {
    "Purimaddha Pachchakhan",
    "Navkar 2 Prahar (6 hours) after sunrise",
    "Sit in meditative posture with closed fist. Recite Navkar 3x. Then eat or drink."
  },
  {
    "Avaddha Pachchakhan",
    "Navkar 3 Prahar (9 hours) after sunrise",
    "Sit in meditative posture with closed fist. Recite Navkar 3x. Then eat or drink."
  },
  {
    "Ekasanu/Biyasanu Pachchakhan",
    "Eat in one or two sittings daily",
    "Restrict eating to one (Ekasanu) or two (Biyasanu) sittings per day."
  },
  {
    "Ayambil Nivi Pachchakhan",
    "Pure diet: cereals & pulses only",
    "Eat cereals and pulses (not sprouted), spice-free, boiled. Avoid dairy, sugar, oil, raw vegetables, fruits, sweets."
  },
  {
    "Upavas (Tivihar) Pachchakhan",
    "Fast from food only, 1 full day",
    "Fast from sunset to sunrise (next day). Water allowed."
  },
  {
    "Upavas (Chovihar) Pachchakhan",
    "Fast from food & water, 1 full day",
    "Fast from both food and water, sunset to sunrise. Water not allowed."
  },
  {
    "Panhar Pachchakhan",
    "Day fast, break at sunset",
    "Say during Pratikraman when fasting."
  },
  {
    "Chauvihar Pachchakhan",
    "No food/water/medication from sunset to sunrise",
    "Automatically do this if doing other Pachkhans."
  },
  {
    "Tivihar Pachchakhan",
    "No food/medication from sunset to sunrise",
    "Take during Pratikraman."
  },
  {
    "Duvihar Pachchakhan",
    "No food from sunset to sunrise",
    "Take during Pratikraman."
  },
  {
    "Dharna Abhigrah Pachchakhan",
    "Customized personal vow",
    "Take during Pratikraman."
  },
};

#endif
