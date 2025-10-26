<template>
  <div class="space-y-6">
    <!-- Forecast Controls -->
    <div class="bg-white rounded-xl shadow-sm p-4">
      <div class="flex flex-wrap items-center justify-between gap-4">
        <div class="flex items-center space-x-4">
          <label class="text-sm font-medium text-gray-700">Forecast Days:</label>
          <select 
            v-model="localParams.days"
            @change="updateParams"
            class="px-3 py-1 border border-gray-300 rounded-md text-sm focus:ring-2 focus:ring-blue-500 focus:border-transparent"
          >
            <option :value="3">3 Days</option>
            <option :value="5">5 Days</option>
            <option :value="7">7 Days</option>
            <option :value="10">10 Days</option>
          </select>
        </div>
        
        <div class="flex items-center space-x-2">
          <button
            @click="refreshForecast"
            :disabled="loading"
            class="px-4 py-2 bg-blue-500 text-white rounded-lg hover:bg-blue-600 disabled:opacity-50 disabled:cursor-not-allowed transition-colors flex items-center space-x-2"
          >
            <RefreshCw :class="['w-4 h-4', loading ? 'animate-spin' : '']" />
            <span>{{ loading ? 'Loading...' : 'Refresh' }}</span>
          </button>
        </div>
      </div>
    </div>

    <!-- Error Message -->
    <div v-if="error" class="bg-red-50 border border-red-200 rounded-xl p-4">
      <div class="flex items-center space-x-2 text-red-700">
        <AlertCircle class="w-5 h-5" />
        <span class="font-medium">Error loading forecast</span>
      </div>
      <p class="text-red-600 text-sm mt-1">{{ error }}</p>
    </div>

    <!-- Loading State -->
    <div v-if="loading" class="bg-white rounded-xl shadow-lg p-8">
      <div class="flex items-center justify-center space-x-3">
        <div class="animate-spin rounded-full h-8 w-8 border-b-2 border-blue-500"></div>
        <span class="text-gray-600">Loading weather forecast...</span>
      </div>
    </div>

    <!-- Forecast Content -->
    <template v-if="!loading && forecast">
      <!-- Debug Info -->
      <!-- <div class="bg-yellow-50 border border-yellow-200 rounded-xl p-4 mb-4">
        <h4 class="font-medium text-yellow-800">Debug Info:</h4>
        <p class="text-sm text-yellow-700">Forecast days: {{ forecast?.forecast?.forecastday?.length || 0 }}</p>
        <p class="text-sm text-yellow-700">Location: {{ forecast?.location?.name || 'Unknown' }}</p>
        <p class="text-sm text-yellow-700">First day data: {{ forecast?.forecast?.forecastday?.[0]?.date || 'No data' }}</p>
      </div> -->

      <!-- Trend Chart -->
      <ForecastChart 
        v-if="forecast && forecast.forecast && forecast.forecast.forecastday && forecast.forecast.forecastday.length > 0"
        :forecast="forecast.forecast.forecastday"
        :temperature-unit="temperatureUnit"
        :wind-unit="windUnit"
      />
      
      <!-- Debug: Show if chart component is not rendered -->
      <div v-else class="bg-red-50 border border-red-200 rounded-xl p-4">
        <p class="text-sm text-red-700">Chart not rendered - missing forecast data</p>
        <p class="text-xs text-red-600">Forecast: {{ !!forecast }}, Forecastday: {{ forecast?.forecast?.forecastday?.length || 0 }}</p>
      </div>

      <!-- Forecast Days -->
      <div class="space-y-4">
        <h3 class="text-lg font-semibold text-gray-900 flex items-center">
          <Calendar class="w-5 h-5 mr-2" />
          Daily Forecast
        </h3>
        
        <div class="space-y-3">
          <ForecastDayCard
            v-for="day in forecast.forecast.forecastday"
            :key="day.date"
            :day="day"
            :temperature-unit="temperatureUnit"
            :wind-unit="windUnit"
          />
        </div>
      </div>
    </template>

    <!-- No Data State -->
    <div v-if="!loading && !forecast && !error" class="bg-white rounded-xl shadow-lg p-8 text-center">
      <div class="text-gray-500">
        <CloudOff class="w-12 h-12 mx-auto mb-4" />
        <p>Enter a location to view the weather forecast</p>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, reactive, watch, onMounted } from 'vue'
import { RefreshCw, AlertCircle, Calendar, CloudOff } from 'lucide-vue-next'
import ForecastChart from './ForecastChart.vue'
import ForecastDayCard from './ForecastDayCard.vue'
import api from '@/api/client'
import type { TemperatureUnit, WindUnit, ForecastRequest, ForecastResponse } from '@/types/weather'

interface Props {
  location: string
  temperatureUnit: TemperatureUnit
  windUnit: WindUnit
  forecastParams: Omit<ForecastRequest, 'location'>
  refreshTrigger?: number
}

const props = defineProps<Props>()

const emit = defineEmits<{
  updateParams: [params: Omit<ForecastRequest, 'location'>]
}>()

const loading = ref(false)
const error = ref<string | null>(null)
const forecast = ref<ForecastResponse | null>(null)

const localParams = reactive({ ...props.forecastParams })

const updateParams = () => {
  emit('updateParams', { ...localParams })
}

const refreshForecast = async () => {
  if (!props.location.trim()) {
    error.value = 'Please enter a location'
    return
  }

  loading.value = true
  error.value = null

  try {
    const forecastRequest: ForecastRequest = {
      location: props.location,
      days: localParams.days,
      include_aqi: localParams.include_aqi || false,
      include_alerts: localParams.include_alerts || false,
      include_hourly: true // Always include hourly for detailed cards
    }

    console.log('Fetching forecast with request:', forecastRequest)
    const response = await api.getForecast(forecastRequest)
    console.log('Received forecast response:', response)
    forecast.value = response
  } catch (err: any) {
    console.error('Failed to fetch forecast:', err)
    
    // For testing purposes, let's use mock data when API fails
    console.log('Using mock data for chart testing...')
    const mockData = createMockForecastData()
    console.log('Created mock forecast data:', mockData)
    forecast.value = mockData
    
    if (err.response?.data?.error?.message) {
      error.value = `${err.response.data.error.message} (Using mock data for demonstration)`
    } else if (err.response?.status === 404) {
      error.value = 'Location not found. Showing sample data for demonstration.'
    } else if (err.response?.status >= 500) {
      error.value = 'Weather service temporarily unavailable. Showing sample data.'
    } else {
      error.value = 'Failed to load live data. Showing sample forecast.'
    }
  } finally {
    loading.value = false
  }
}

const createMockForecastData = (): ForecastResponse => {
  const today = new Date()
  const forecastDays = []
  
  for (let i = 0; i < localParams.days; i++) {
    const date = new Date(today)
    date.setDate(today.getDate() + i)
    
    const temp_base = 20 + Math.sin(i * 0.5) * 5
    const wind_base = 15 + Math.random() * 10
    const precip_base = Math.random() * 5
    
    forecastDays.push({
      date: date.toISOString().split('T')[0],
      date_epoch: Math.floor(date.getTime() / 1000),
      day: {
        maxtemp_c: temp_base + 5,
        maxtemp_f: (temp_base + 5) * 9/5 + 32,
        mintemp_c: temp_base - 3,
        mintemp_f: (temp_base - 3) * 9/5 + 32,
        avgtemp_c: temp_base,
        avgtemp_f: temp_base * 9/5 + 32,
        maxwind_mph: wind_base * 0.621371,
        maxwind_kph: wind_base,
        totalprecip_mm: precip_base,
        totalprecip_in: precip_base * 0.0393701,
        avghumidity: 60 + Math.random() * 20,
        daily_will_it_rain: precip_base > 1 ? 1 : 0,
        daily_chance_of_rain: Math.round(precip_base * 20),
        uv: 3 + Math.random() * 5,
        condition: {
          text: precip_base > 2 ? 'Light rain' : precip_base > 1 ? 'Partly cloudy' : 'Sunny',
          icon: '//cdn.weatherapi.com/weather/64x64/day/116.png',
          code: 1003
        }
      },
      astro: {
        sunrise: '06:30 AM',
        sunset: '07:45 PM',
        moonrise: '09:15 PM',
        moonset: '06:45 AM',
        moon_phase: 'Waning Crescent',
        moon_illumination: Math.round(Math.random() * 100)
      },
      hour: []
    })
  }
  
  return {
    location: {
      name: props.location,
      region: 'Demo Region',
      country: 'Demo Country',
      lat: 51.5,
      lon: 0.12,
      tz_id: 'Europe/London',
      localtime_epoch: Math.floor(Date.now() / 1000),
      localtime: new Date().toISOString()
    },
    forecast: {
      forecastday: forecastDays
    }
  }
}

// Watch for refresh trigger (for tab switches and manual refreshes)
watch(() => props.refreshTrigger, () => {
  if (props.location.trim()) {
    refreshForecast()
  }
})

// Watch for location changes - clear data when location is empty, but rely on refreshTrigger for loading
watch(() => props.location, (newLocation) => {
  if (!newLocation.trim()) {
    forecast.value = null
    error.value = null
  }
  // Don't auto-refresh here - let refreshTrigger handle it to avoid conflicts
})

// Watch for parameter changes - only refresh if we have data already
watch(() => props.forecastParams, (newParams) => {
  Object.assign(localParams, newParams)
  // Only refresh if we already have forecast data (user has searched)
  if (props.location.trim() && forecast.value) {
    refreshForecast()
  }
}, { deep: true })

// Load data on mount if location is already set
onMounted(() => {
  if (props.location.trim()) {
    refreshForecast()
  }
})
</script>