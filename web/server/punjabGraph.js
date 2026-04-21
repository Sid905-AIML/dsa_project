export const punjabGraph = {
  cities: [
    { id: "amritsar", name: "Amritsar", x: 120, y: 120, kind: "major" },
    { id: "tarn_taran", name: "Tarn Taran", x: 135, y: 150, kind: "minor" },
    { id: "gurdaspur", name: "Gurdaspur", x: 175, y: 95, kind: "minor" },
    { id: "pathankot", name: "Pathankot", x: 235, y: 75, kind: "major" },
    { id: "hoshiarpur", name: "Hoshiarpur", x: 260, y: 150, kind: "minor" },
    { id: "kapurthala", name: "Kapurthala", x: 210, y: 200, kind: "minor" },
    { id: "jalandhar", name: "Jalandhar", x: 260, y: 220, kind: "major" },
    { id: "ludhiana", name: "Ludhiana", x: 340, y: 260, kind: "major" },
    { id: "moga", name: "Moga", x: 290, y: 320, kind: "minor" },
    { id: "firozpur", name: "Firozpur", x: 205, y: 360, kind: "minor" },
    { id: "fazilka", name: "Fazilka", x: 130, y: 430, kind: "minor" },
    { id: "abohar", name: "Abohar", x: 175, y: 460, kind: "minor" },
    { id: "bathinda", name: "Bathinda", x: 300, y: 430, kind: "major" },
    { id: "faridkot", name: "Faridkot", x: 255, y: 395, kind: "minor" },
    { id: "muktsar", name: "Sri Muktsar Sahib", x: 235, y: 450, kind: "minor" },
    { id: "mansa", name: "Mansa", x: 360, y: 455, kind: "minor" },
    { id: "barnala", name: "Barnala", x: 385, y: 380, kind: "minor" },
    { id: "sangrur", name: "Sangrur", x: 430, y: 360, kind: "minor" },
    { id: "patiala", name: "Patiala", x: 500, y: 350, kind: "major" },
    { id: "malerkotla", name: "Malerkotla", x: 450, y: 320, kind: "minor" },
    { id: "rupnagar", name: "Rupnagar", x: 455, y: 230, kind: "minor" },
    { id: "mohali", name: "Mohali", x: 520, y: 260, kind: "major" },
    { id: "chandigarh", name: "Chandigarh", x: 560, y: 240, kind: "major" }
  ],
  roads: [
    // north-west cluster
    ["amritsar", "tarn_taran"],
    ["amritsar", "gurdaspur"],
    ["gurdaspur", "pathankot"],
    ["pathankot", "hoshiarpur"],
    ["hoshiarpur", "jalandhar"],
    ["tarn_taran", "kapurthala"],
    ["kapurthala", "jalandhar"],
    ["amritsar", "kapurthala"],

    // central belt
    ["jalandhar", "ludhiana"],
    ["ludhiana", "rupnagar"],
    ["rupnagar", "mohali"],
    ["mohali", "chandigarh"],
    ["rupnagar", "chandigarh"],
    ["ludhiana", "malerkotla"],
    ["malerkotla", "sangrur"],
    ["malerkotla", "patiala"],
    ["sangrur", "patiala"],

    // west-south belt
    ["ludhiana", "moga"],
    ["moga", "firozpur"],
    ["firozpur", "fazilka"],
    ["fazilka", "abohar"],
    ["abohar", "muktsar"],
    ["muktsar", "bathinda"],
    ["faridkot", "bathinda"],
    ["faridkot", "moga"],
    ["faridkot", "muktsar"],

    // south-east belt
    ["bathinda", "barnala"],
    ["barnala", "sangrur"],
    ["barnala", "mansa"],
    ["mansa", "bathinda"],
    ["patiala", "mohali"],
    ["ludhiana", "patiala"]
  ]
};

