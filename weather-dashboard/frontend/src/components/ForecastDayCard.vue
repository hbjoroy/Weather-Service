<template>
  <div class="bg-white rounded-lg shadow-md border border-gray-200 overflow-hidden transition-all duration-200 hover:shadow-lg">
    <!-- Summary Card (Always Visible) -->
    <div 
      @click="toggleExpanded"
      class="p-4 cursor-pointer hover:bg-gray-50 transition-colors"
    >
      <div class="flex items-center justify-between">
        <!-- Date and Day -->
        <div class="flex-1">
          <h3 class="font-semibold text-gray-900">{{ dayName }}</h3>
          <p class="text-sm text-gray-500">{{ formattedDate }}</p>
        </div>

        <!-- Weather Icon and Condition -->
        <div class="flex items-center space-x-3 flex-1 justify-center">
          <img 
            :src="getWeatherIconUrl(day.day.condition.icon)"
            :alt="day.day.condition.text"
            class="w-12 h-12"
          />
          <div class="text-center">
            <p class="text-sm font-medium text-gray-700">{{ day.day.condition.text }}</p>
            <p class="text-xs text-gray-500">{{ Math.round(day.day.daily_chance_of_rain) }}% rain</p>
          </div>
        </div>

        <!-- Temperature Range -->
        <div class="flex-1 text-right">
          <div class="flex items-center justify-end space-x-2">
            <span class="text-xl font-bold text-gray-900">{{ getHighTemp() }}°</span>
            <span class="text-lg text-gray-500">{{ getLowTemp() }}°</span>
          </div>
          <div class="flex items-center justify-end space-x-4 mt-1">
            <!-- Wind -->
            <div class="flex items-center space-x-1 text-xs text-gray-500">
              <Wind class="w-3 h-3" />
              <span>{{ getDayWindSpeed() }} {{ getWindUnitLabel() }} {{ getDominantWindDirection() }}</span>
            </div>
            <!-- Precipitation -->
            <div class="flex items-center space-x-1 text-xs text-gray-500">
              <Droplets class="w-3 h-3" />
              <span>{{ day.day.totalprecip_mm.toFixed(1) }} mm</span>
            </div>
          </div>
        </div>

        <!-- Expand/Collapse Icon -->
        <div class="ml-4">
          <ChevronDown 
            :class="[
              'w-5 h-5 text-gray-400 transition-transform duration-200',
              isExpanded ? 'transform rotate-180' : ''
            ]"
          />
        </div>
      </div>
    </div>

    <!-- Detailed Information (Expandable) -->
    <div 
      v-if="isExpanded"
      class="border-t border-gray-200 bg-gray-50"
    >
      <div class="p-4 space-y-4">
        <!-- Detailed Weather Stats -->
        <div class="grid grid-cols-2 md:grid-cols-4 gap-4">
          <div class="text-center">
            <div class="flex items-center justify-center space-x-1 text-gray-500 text-xs mb-1">
              <Thermometer class="w-3 h-3" />
              <span>Feels Like</span>
            </div>
            <p class="font-semibold">{{ getFeelsLike() }}°</p>
          </div>
          
          <div class="text-center">
            <div class="flex items-center justify-center space-x-1 text-gray-500 text-xs mb-1">
              <Droplets class="w-3 h-3" />
              <span>Humidity</span>
            </div>
            <p class="font-semibold">{{ Math.round(day.day.avghumidity) }}%</p>
          </div>
          
          <div class="text-center">
            <div class="flex items-center justify-center space-x-1 text-gray-500 text-xs mb-1">
              <Eye class="w-3 h-3" />
              <span>UV Index</span>
            </div>
            <p class="font-semibold">{{ day.day.uv }}</p>
          </div>
          
          <div class="text-center">
            <div class="flex items-center justify-center space-x-1 text-gray-500 text-xs mb-1">
              <Wind class="w-3 h-3" />
              <span>Avg Wind</span>
            </div>
            <p class="font-semibold">{{ getDayWindSpeed() }} {{ getWindUnitLabel() }}</p>
            <p class="text-xs text-gray-500">{{ getDominantWindDirection() }}</p>
          </div>
        </div>

        <!-- Astronomy Information -->
        <div class="border-t border-gray-200 pt-4">
          <h4 class="text-sm font-medium text-gray-700 mb-3 flex items-center">
            <Sun class="w-4 h-4 mr-2" />
            Sun & Moon
          </h4>
          <div class="grid grid-cols-2 md:grid-cols-4 gap-4 text-sm">
            <div>
              <p class="text-gray-500">Sunrise</p>
              <p class="font-medium">{{ day.astro.sunrise }}</p>
            </div>
            <div>
              <p class="text-gray-500">Sunset</p>
              <p class="font-medium">{{ day.astro.sunset }}</p>
            </div>
            <div>
              <p class="text-gray-500">Moon Phase</p>
              <p class="font-medium">{{ day.astro.moon_phase }}</p>
            </div>
            <div>
              <p class="text-gray-500">Moon Illumination</p>
              <p class="font-medium">{{ day.astro.moon_illumination }}%</p>
            </div>
          </div>
        </div>

        <!-- Hourly Forecast (if available) -->
        <div v-if="day.hour && day.hour.length > 0" class="border-t border-gray-200 pt-4">
          <h4 class="text-sm font-medium text-gray-700 mb-3 flex items-center">
            <Clock class="w-4 h-4 mr-2" />
            Hourly Forecast
          </h4>
          <div class="flex space-x-4 overflow-x-auto pb-2">
            <div 
              v-for="hour in getHourlyForecast()" 
              :key="hour.time"
              class="flex-none text-center min-w-20"
            >
              <p class="text-xs text-gray-500 mb-1">{{ formatHour(hour.time) }}</p>
              <img 
                :src="getWeatherIconUrl(hour.condition.icon)"
                :alt="hour.condition.text"
                class="w-8 h-8 mx-auto mb-1"
              />
              <p class="text-sm font-medium">{{ getHourlyTemp(hour) }}°</p>
              <div class="flex items-center justify-center text-xs text-gray-500 mb-1">
                <Wind class="w-3 h-3 mr-1" />
                <span>{{ getHourlyWindSpeed(hour) }}</span>
              </div>
              <p class="text-xs text-gray-400">{{ hour.wind_dir || 'Variable' }}</p>
              <p class="text-xs text-gray-500">{{ hour.chance_of_rain }}%</p>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed } from 'vue'
import { 
  ChevronDown, 
  Wind, 
  Droplets, 
  Thermometer, 
  Eye, 
  Sun, 
  Clock 
} from 'lucide-vue-next'
import type { ForecastDaily, ForecastHour, TemperatureUnit, WindUnit } from '@/types/weather'

interface Props {
  day: ForecastDaily
  temperatureUnit: TemperatureUnit
  windUnit: WindUnit
}

const props = defineProps<Props>()

// ============================================================================
// UI STATE
// ============================================================================

const isCardExpanded = ref(false)

const toggleCardExpansion = (): void => {
  isCardExpanded.value = !isCardExpanded.value
}

// ============================================================================
// DATE FORMATTING
// ============================================================================

const fullDayName = computed(() => {
  const date = new Date(props.day.date)
  return date.toLocaleDateString('en-US', { weekday: 'long' })
})

const shortDateDisplay = computed(() => {
  const date = new Date(props.day.date)
  return date.toLocaleDateString('en-US', { month: 'short', day: 'numeric' })
})

const formatHourDisplay = (timeStr: string): string => {
  const date = new Date(timeStr)
  return date.toLocaleTimeString('en-US', { 
    hour: 'numeric', 
    hour12: true 
  })
}

// ============================================================================
// TEMPERATURE EXTRACTION
// ============================================================================

const extractDayHighTemperature = (): number => {
  const temp = props.temperatureUnit === 'celsius' 
    ? props.day.day.maxtemp_c 
    : props.day.day.maxtemp_f
  return Math.round(temp)
}

const extractDayLowTemperature = (): number => {
  const temp = props.temperatureUnit === 'celsius' 
    ? props.day.day.mintemp_c 
    : props.day.day.mintemp_f
  return Math.round(temp)
}

const extractFeelsLikeTemperature = (): number => {
  const temp = props.temperatureUnit === 'celsius' 
    ? props.day.day.avgtemp_c 
    : props.day.day.avgtemp_f
  return Math.round(temp)
}

const extractHourlyTemperature = (hour: ForecastHour): number => {
  const temp = props.temperatureUnit === 'celsius' ? hour.temp_c : hour.temp_f
  return Math.round(temp)
}

// ============================================================================
// WIND SPEED CONVERSION AND CALCULATION
// ============================================================================

const DAYTIME_START_HOUR = 6
const DAYTIME_END_HOUR = 18
const KNOTS_PER_KPH = 0.539957
const METERS_PER_SECOND_PER_KPH = 1 / 3.6

const convertKphToSelectedWindUnit = (kph: number): number => {
  switch (props.windUnit) {
    case 'knots':
      return kph * KNOTS_PER_KPH
    case 'ms':
      return kph * METERS_PER_SECOND_PER_KPH
    case 'kmh':
    default:
      return kph
  }
}

const getSelectedWindUnitLabel = (): string => {
  const windUnitLabels = {
    'knots': 'knots',
    'ms': 'm/s',
    'kmh': 'km/h'
  }
  return windUnitLabels[props.windUnit] || 'km/h'
}

const filterDaytimeHourData = () => {
  if (!props.day.hour || props.day.hour.length === 0) return []
  
  return props.day.hour.filter(hour => {
    const hourOfDay = new Date(hour.time).getHours()
    return hourOfDay >= DAYTIME_START_HOUR && hourOfDay <= DAYTIME_END_HOUR
  })
}

const calculateDayAverageWindSpeed = (): number => {
  const daytimeHours = filterDaytimeHourData()
  
  if (daytimeHours.length > 0) {
    const totalWindKph = daytimeHours.reduce((sum, hour) => sum + hour.wind_kph, 0)
    const averageWindKph = totalWindKph / daytimeHours.length
    return Math.round(convertKphToSelectedWindUnit(averageWindKph))
  }
  
  // Fallback to maximum wind speed if no hourly data available
  return Math.round(convertKphToSelectedWindUnit(props.day.day.maxwind_kph))
}

const findMostCommonDirection = (directionCounts: Record<string, number>): string => {
  let maxCount = 0
  let mostCommonDirection = ''
  
  for (const [direction, count] of Object.entries(directionCounts)) {
    if (count > maxCount) {
      maxCount = count
      mostCommonDirection = direction
    }
  }
  
  return mostCommonDirection || 'Variable'
}

const calculateDominantWindDirection = (): string => {
  const daytimeHours = filterDaytimeHourData()
  
  if (daytimeHours.length === 0) return 'Variable'
  
  // Count frequency of each wind direction
  const directionCounts: Record<string, number> = {}
  daytimeHours.forEach(hour => {
    if (hour.wind_dir) {
      directionCounts[hour.wind_dir] = (directionCounts[hour.wind_dir] || 0) + 1
    }
  })
  
  return findMostCommonDirection(directionCounts)
}

const extractHourlyWindSpeed = (hour: ForecastHour): number => {
  return Math.round(convertKphToSelectedWindUnit(hour.wind_kph))
}

// ============================================================================
// ICON AND DISPLAY HELPERS
// ============================================================================

const buildWeatherIconUrl = (iconPath: string): string => {
  // WeatherAPI.com provides icons with protocol-relative URLs
  return iconPath.startsWith('//') ? `https:${iconPath}` : iconPath
}

// ============================================================================
// LEGACY FUNCTION ALIASES (for template compatibility)
// ============================================================================

const isExpanded = isCardExpanded
const toggleExpanded = toggleCardExpansion
const dayName = fullDayName
const formattedDate = shortDateDisplay
const getHighTemp = extractDayHighTemperature
const getLowTemp = extractDayLowTemperature
const getFeelsLike = extractFeelsLikeTemperature
const getHourlyTemp = extractHourlyTemperature
const convertWindSpeed = convertKphToSelectedWindUnit
const getWindUnitLabel = getSelectedWindUnitLabel
const getDayWindSpeed = calculateDayAverageWindSpeed
const getDominantWindDirection = calculateDominantWindDirection
const getHourlyWindSpeed = extractHourlyWindSpeed
const getWeatherIconUrl = buildWeatherIconUrl
const formatHour = formatHourDisplay

// ============================================================================
// HOURLY FORECAST FILTERING
// ============================================================================

const filterHourlyForecastData = () => {
  if (!props.day.hour) return []
  
  // Show every 3 hours (results in 8 data points for 24 hours)
  return props.day.hour.filter((_, index) => index % 3 === 0)
}

// Legacy alias
const getHourlyForecast = filterHourlyForecastData
</script>