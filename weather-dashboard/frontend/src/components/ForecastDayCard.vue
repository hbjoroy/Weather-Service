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

const isExpanded = ref(false)

const toggleExpanded = () => {
  isExpanded.value = !isExpanded.value
}

const dayName = computed(() => {
  const date = new Date(props.day.date)
  return date.toLocaleDateString('en-US', { weekday: 'long' })
})

const formattedDate = computed(() => {
  const date = new Date(props.day.date)
  return date.toLocaleDateString('en-US', { month: 'short', day: 'numeric' })
})

const getHighTemp = () => {
  return Math.round(props.temperatureUnit === 'celsius' 
    ? props.day.day.maxtemp_c 
    : props.day.day.maxtemp_f)
}

const getLowTemp = () => {
  return Math.round(props.temperatureUnit === 'celsius' 
    ? props.day.day.mintemp_c 
    : props.day.day.mintemp_f)
}

const getFeelsLike = () => {
  return Math.round(props.temperatureUnit === 'celsius' 
    ? props.day.day.avgtemp_c 
    : props.day.day.avgtemp_f)
}

const getHourlyTemp = (hour: ForecastHour) => {
  return Math.round(props.temperatureUnit === 'celsius' ? hour.temp_c : hour.temp_f)
}

const convertWindSpeed = (kph: number) => {
  switch (props.windUnit) {
    case 'knots':
      return kph * 0.539957 // Convert km/h to knots
    case 'ms':
      return kph / 3.6 // Convert km/h to m/s
    case 'kmh':
    default:
      return kph
  }
}

const getWindUnitLabel = () => {
  switch (props.windUnit) {
    case 'knots':
      return 'knots'
    case 'ms':
      return 'm/s'
    case 'kmh':
    default:
      return 'km/h'
  }
}

const getDayWindSpeed = () => {
  // Calculate daytime average wind speed (6 AM to 6 PM)
  if (props.day.hour && props.day.hour.length > 0) {
    const daytimeHours = props.day.hour.filter(hour => {
      const hourTime = new Date(hour.time).getHours()
      return hourTime >= 6 && hourTime <= 18
    })
    
    if (daytimeHours.length > 0) {
      const totalWind = daytimeHours.reduce((sum, hour) => sum + hour.wind_kph, 0)
      const avgWind = totalWind / daytimeHours.length
      return Math.round(convertWindSpeed(avgWind))
    }
  }
  
  // Fallback to max wind if hourly data not available
  return Math.round(convertWindSpeed(props.day.day.maxwind_kph))
}

const getDominantWindDirection = () => {
  // Calculate dominant wind direction during daytime
  if (props.day.hour && props.day.hour.length > 0) {
    const daytimeHours = props.day.hour.filter(hour => {
      const hourTime = new Date(hour.time).getHours()
      return hourTime >= 6 && hourTime <= 18
    })
    
    if (daytimeHours.length > 0) {
      // Count occurrences of each wind direction
      const directionCounts: Record<string, number> = {}
      daytimeHours.forEach(hour => {
        if (hour.wind_dir) {
          directionCounts[hour.wind_dir] = (directionCounts[hour.wind_dir] || 0) + 1
        }
      })
      
      // Find most common direction
      let maxCount = 0
      let dominantDirection = ''
      for (const [direction, count] of Object.entries(directionCounts)) {
        if (count > maxCount) {
          maxCount = count
          dominantDirection = direction
        }
      }
      
      return dominantDirection || 'Variable'
    }
  }
  
  return 'Variable'
}

const getHourlyWindSpeed = (hour: ForecastHour) => {
  return Math.round(convertWindSpeed(hour.wind_kph))
}

const getWeatherIconUrl = (iconPath: string) => {
  // WeatherAPI.com provides icons with full URLs
  return iconPath.startsWith('//') ? `https:${iconPath}` : iconPath
}

const formatHour = (timeStr: string) => {
  const date = new Date(timeStr)
  return date.toLocaleTimeString('en-US', { 
    hour: 'numeric', 
    hour12: true 
  }).replace(' ', '').toLowerCase()
}

const getHourlyForecast = () => {
  if (!props.day.hour) return []
  
  // Show every 3 hours (8 data points for 24 hours)
  return props.day.hour.filter((_, index) => index % 3 === 0)
}
</script>