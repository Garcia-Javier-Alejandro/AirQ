/** @type {import('tailwindcss').Config} */
module.exports = {
  content: ["./public/**/*.{html,js}"],
  theme: {
    extend: {
      colors: {
        aq: {
          good: '#10b981',      // Green
          fair: '#f59e0b',      // Amber
          poor: '#ef4444',      // Red
          excellent: '#34d399', // Lighter green
        }
      }
    },
  },
  plugins: [],
}
