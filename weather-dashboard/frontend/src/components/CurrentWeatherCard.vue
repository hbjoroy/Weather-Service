<template>
  <div class="bg-white rounded-xl shadow-lg p-6 max-w-2xl mx-auto">
    <!-- Location and Time -->
    <div class="text-center mb-6">
      <h2 class="text-2xl font-bold text-gray-900">{{ weather.location.name }}</h2>
      <p class="text-gray-600">{{ weather.location.region }}, {{ weather.location.country }}</p>
      <p class="text-sm text-gray-500 mt-1">{{ formatLocalTime(weather.location.localtime) }}</p>
    </div>

    <!-- Current Conditions -->
    <div class="flex items-center justify-center space-x-8 mb-6">
      <!-- Weather Icon and Condition -->
      <div class="text-center">
        <img 
          :src="'https:' + weather.current.condition.icon" 
          :alt="weather.current.condition.text"
          class="w-20 h-20 mx-auto mb-2"
        />
        <p class="text-lg font-medium text-gray-800">{{ weather.current.condition.text }}</p>
      </div>

      <!-- Temperature -->
      <div class="text-center">
        <div class="text-5xl font-bold text-gray-900 mb-2">
          {{ getTemperature(weather.current.temp_c, weather.current.temp_f) }}°
        </div>
        <p class="text-gray-600">{{ temperatureUnit === 'celsius' ? 'Celsius' : 'Fahrenheit' }}</p>
        <p class="text-sm text-gray-500 mt-1">
          Feels like {{ getTemperature(weather.current.feelslike_c, weather.current.feelslike_f) }}°
        </p>
      </div>
    </div>

    <!-- Weather Details Grid -->
    <div class="grid grid-cols-2 md:grid-cols-4 gap-4">
      <div class="bg-gray-50 rounded-lg p-4 text-center">
        <div class="flex items-center justify-center mb-2">
          <Wind class="w-5 h-5 text-blue-500" />
        </div>
        <p class="text-sm text-gray-600">Wind</p>
        <p class="font-semibold">{{ getWindSpeed(weather.current.wind_kph, weather.current.wind_mph) }}</p>
        <p class="text-xs text-gray-500">{{ weather.current.wind_dir }}</p>
      </div>

      <div class="bg-gray-50 rounded-lg p-4 text-center">
        <div class="flex items-center justify-center mb-2">
          <Droplets class="w-5 h-5 text-blue-500" />
        </div>
        <p class="text-sm text-gray-600">Humidity</p>
        <p class="font-semibold">{{ weather.current.humidity }}%</p>
      </div>

      <div class="bg-gray-50 rounded-lg p-4 text-center">
        <div class="flex items-center justify-center mb-2">
          <Gauge class="w-5 h-5 text-blue-500" />
        </div>
        <p class="text-sm text-gray-600">Pressure</p>
        <p class="font-semibold">{{ weather.current.pressure_mb }} mb</p>
      </div>

      <div class="bg-gray-50 rounded-lg p-4 text-center">
        <div class="flex items-center justify-center mb-2">
          <Eye class="w-5 h-5 text-blue-500" />
        </div>
        <p class="text-sm text-gray-600">Visibility</p>
        <p class="font-semibold">{{ getDistance(weather.current.vis_km, weather.current.vis_miles) }}</p>
      </div>

      <div class="bg-gray-50 rounded-lg p-4 text-center">
        <div class="flex items-center justify-center mb-2">
          <Sun class="w-5 h-5 text-yellow-500" />
        </div>
        <p class="text-sm text-gray-600">UV Index</p>
        <p class="font-semibold">{{ weather.current.uv }}</p>
      </div>

      <div class="bg-gray-50 rounded-lg p-4 text-center">
        <div class="flex items-center justify-center mb-2">
          <Cloud class="w-5 h-5 text-gray-500" />
        </div>
        <p class="text-sm text-gray-600">Cloud Cover</p>
        <p class="font-semibold">{{ weather.current.cloud }}%</p>
      </div>

      <div class="bg-gray-50 rounded-lg p-4 text-center">
        <div class="flex items-center justify-center mb-2">
          <CloudRain class="w-5 h-5 text-blue-500" />
        </div>
        <p class="text-sm text-gray-600">Precipitation</p>
        <p class="font-semibold">{{ getPrecipitation(weather.current.precip_mm, weather.current.precip_in) }}</p>
      </div>

      <div class="bg-gray-50 rounded-lg p-4 text-center">
        <div class="flex items-center justify-center mb-2">
          <Zap class="w-5 h-5 text-yellow-500" />
        </div>
        <p class="text-sm text-gray-600">Gust</p>
        <p class="font-semibold">{{ getWindSpeed(weather.current.gust_kph, weather.current.gust_mph) }}</p>
      </div>
    </div>

    <!-- Last Updated -->
    <div class="text-center mt-6 pt-4 border-t border-gray-200">
      <p class="text-sm text-gray-500">
        Last updated: {{ formatLastUpdated(weather.current.last_updated) }}
      </p>
    </div>
  </div>
</template>

<script setup lang="ts">
import { Wind, Droplets, Gauge, Eye, Sun, Cloud, CloudRain, Zap } from 'lucide-vue-next'
import type { WeatherResponse, TemperatureUnit, WindUnit } from '@/types/weather'

interface Props {
  weather: WeatherResponse
  temperatureUnit: TemperatureUnit
  windUnit: WindUnit
}

const props = defineProps<Props>()

// Helper functions
const getTemperature = (celsius: number, fahrenheit: number): string => {
  const temp = props.temperatureUnit === 'celsius' ? celsius : fahrenheit
  return Math.round(temp).toString()
}

const convertWindSpeed = (kph: number): number => {
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

const getWindUnitLabel = (): string => {
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

const getWindSpeed = (kph: number, mph: number): string => {
  const speed = convertWindSpeed(kph)
  const unit = getWindUnitLabel()
  return `${Math.round(speed)} ${unit}`
}

const getDistance = (km: number, miles: number): string => {
  const distance = props.temperatureUnit === 'celsius' ? km : miles
  const unit = props.temperatureUnit === 'celsius' ? 'km' : 'mi'
  return `${Math.round(distance)} ${unit}`
}

const getPrecipitation = (mm: number, inches: number): string => {
  const precip = props.temperatureUnit === 'celsius' ? mm : inches
  const unit = props.temperatureUnit === 'celsius' ? 'mm' : 'in'
  return `${precip.toFixed(1)} ${unit}`
}

const formatLocalTime = (localtime: string): string => {
  return new Date(localtime).toLocaleString('en-US', {
    weekday: 'long',
    year: 'numeric',
    month: 'long',
    day: 'numeric',
    hour: '2-digit',
    minute: '2-digit'
  })
}

const formatLastUpdated = (lastUpdated: string): string => {
  return new Date(lastUpdated).toLocaleString('en-US', {
    month: 'short',
    day: 'numeric',
    hour: '2-digit',
    minute: '2-digit'
  })
}
</script>