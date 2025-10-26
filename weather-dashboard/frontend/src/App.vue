<template>
  <div id="app" class="min-h-screen bg-gradient-to-br from-blue-50 to-indigo-100">
    <!-- Header -->
    <header class="bg-white shadow-sm border-b border-gray-200">
      <div class="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div class="flex justify-between items-center py-4">
          <div class="flex items-center space-x-4">
            <div class="w-8 h-8 bg-gradient-to-r from-blue-500 to-indigo-600 rounded-lg flex items-center justify-center">
              <CloudSun class="w-5 h-5 text-white" />
            </div>
            <h1 class="text-2xl font-bold text-gray-900">Weather Dashboard</h1>
          </div>
          
          <!-- User Profile -->
          <div class="flex items-center space-x-4">
            <div class="text-right">
              <p class="text-sm font-medium text-gray-900">{{ profile.name }}</p>
              <p class="text-xs text-gray-500">{{ profile.tempUnit === 'celsius' ? 'Celsius' : 'Fahrenheit' }}</p>
            </div>
            <button 
              @click="showProfileModal = true"
              class="p-2 rounded-full bg-gray-100 hover:bg-gray-200 transition-colors"
            >
              <Settings class="w-5 h-5 text-gray-600" />
            </button>
          </div>
        </div>
      </div>
    </header>

    <!-- Main Content -->
    <main class="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      <!-- Location Input -->
      <div class="mb-8">
        <div class="max-w-md mx-auto">
          <label for="location" class="block text-sm font-medium text-gray-700 mb-2">
            Location
          </label>
          <div class="relative">
            <input
              id="location"
              v-model="currentLocation"
              type="text"
              placeholder="Enter city name or coordinates, then click search..."
              class="w-full px-4 py-3 pl-10 pr-4 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            />
            <MapPin class="absolute left-3 top-3.5 w-5 h-5 text-gray-400" />
            <button
              @click="refreshData"
              :disabled="loading || !currentLocation.trim()"
              class="absolute right-2 top-2 px-3 py-1.5 bg-blue-500 text-white rounded-md hover:bg-blue-600 disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
            >
              <Search class="w-4 h-4" />
            </button>
          </div>
        </div>
      </div>

      <!-- Loading State -->
      <div v-if="loading" class="text-center py-12">
        <div class="inline-block animate-spin rounded-full h-8 w-8 border-b-2 border-blue-500"></div>
        <p class="mt-2 text-gray-600">Loading weather data...</p>
      </div>

      <!-- Error State -->
      <div v-else-if="error" class="text-center py-12">
        <AlertCircle class="w-12 h-12 text-red-500 mx-auto mb-4" />
        <h3 class="text-lg font-semibold text-gray-900 mb-2">Error Loading Weather Data</h3>
        <p class="text-gray-600 mb-4">{{ error }}</p>
        <button 
          @click="refreshData"
          class="px-4 py-2 bg-blue-500 text-white rounded-lg hover:bg-blue-600 transition-colors"
        >
          Try Again
        </button>
      </div>

      <!-- Weather Content -->
      <div v-else-if="currentWeather" class="space-y-8">
        <!-- Tabs -->
        <div class="flex justify-center">
          <div class="bg-white rounded-lg p-1 shadow-sm border border-gray-200">
            <button
              @click="activeTab = 'current'"
              :class="[
                'px-6 py-2 rounded-md font-medium transition-colors',
                activeTab === 'current' 
                  ? 'bg-blue-500 text-white shadow-sm' 
                  : 'text-gray-600 hover:text-gray-900'
              ]"
            >
              Current Weather
            </button>
            <button
              @click="activeTab = 'forecast'"
              :class="[
                'px-6 py-2 rounded-md font-medium transition-colors',
                activeTab === 'forecast' 
                  ? 'bg-blue-500 text-white shadow-sm' 
                  : 'text-gray-600 hover:text-gray-900'
              ]"
            >
              Forecast
            </button>
          </div>
        </div>

        <!-- Current Weather Tab -->
        <CurrentWeatherCard 
          v-if="activeTab === 'current'"
          :weather="currentWeather"
          :temperature-unit="profile.tempUnit"
        />

        <!-- Forecast Tab -->
        <ForecastSection
          v-show="activeTab === 'forecast'"
          :location="searchLocation"
          :temperature-unit="profile.tempUnit"
          :wind-unit="profile.windUnit"
          :forecast-params="forecastParams"
          :refresh-trigger="forecastRefreshTrigger"
          @update-params="updateForecastParams"
        />
      </div>
    </main>

    <!-- Profile Modal -->
    <ProfileModal
      v-if="showProfileModal"
      :profile="profile"
      @update="updateProfile"
      @close="showProfileModal = false"
    />
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, computed, watch, nextTick } from 'vue'
import { CloudSun, Settings, MapPin, Search, AlertCircle } from 'lucide-vue-next'
import CurrentWeatherCard from '@/components/CurrentWeatherCard.vue'
import ForecastSection from '@/components/ForecastSection.vue'
import ProfileModal from '@/components/ProfileModal.vue'
import api from '@/api/client'
import type { UserProfile, WeatherResponse, ForecastRequest } from '@/types/weather'

// Reactive state
const profile = ref<UserProfile>({
  name: 'Χαράλαμπους Μπιγγ',
  tempUnit: 'celsius',
  windUnit: 'ms',
  defaultLocation: 'Athens'
})

const currentLocation = ref('')
const searchLocation = ref('') // Central state that triggers data loading
const currentWeather = ref<WeatherResponse | null>(null)
const activeTab = ref<'current' | 'forecast'>('current')
const showProfileModal = ref(false)
const loading = ref(false)
const error = ref('')
const forecastRefreshTrigger = ref(0) // Simple trigger for tab switches

const forecastParams = ref<Omit<ForecastRequest, 'location'>>({
  days: 5,
  include_aqi: false,
  include_alerts: false,
  include_hourly: false
})

// Methods
const loadProfile = async () => {
  try {
    const userProfile = await api.getProfile()
    profile.value = userProfile
    currentLocation.value = userProfile.defaultLocation
  } catch (err) {
    console.error('Failed to load profile:', err)
    // Use default profile if API fails
  }
}

const updateProfile = async (newProfile: UserProfile) => {
  try {
    const updatedProfile = await api.updateProfile(newProfile)
    profile.value = updatedProfile
    showProfileModal.value = false
    
    // Update location but don't auto-refresh
    if (currentLocation.value !== updatedProfile.defaultLocation) {
      currentLocation.value = updatedProfile.defaultLocation
      // Remove automatic refresh - user must click search
    }
  } catch (err) {
    console.error('Failed to update profile:', err)
    error.value = 'Failed to update profile: ' + (err as Error).message
  }
}

const loadCurrentWeather = async () => {
  if (!searchLocation.value.trim()) return
  
  try {
    currentWeather.value = await api.getCurrentWeather(searchLocation.value, forecastParams.value.include_aqi)
  } catch (err) {
    throw new Error('Failed to load current weather: ' + (err as Error).message)
  }
}

const refreshData = async () => {
  if (!currentLocation.value.trim()) return
  
  // Update the central search location - this will trigger data loading in both components
  searchLocation.value = currentLocation.value.trim()
  
  // Wait a moment for the searchLocation watcher to process, then trigger forecast refresh
  await nextTick()
  forecastRefreshTrigger.value++
}

const updateForecastParams = (params: Omit<ForecastRequest, 'location'>) => {
  forecastParams.value = params
}

// Watch for search location changes - load current weather when location changes
watch(searchLocation, async (newLocation) => {
  if (!newLocation.trim()) {
    currentWeather.value = null
    return
  }
  
  loading.value = true
  error.value = ''
  
  try {
    await loadCurrentWeather()
  } catch (err) {
    error.value = (err as Error).message
    currentWeather.value = null
  } finally {
    loading.value = false
  }
})

// Watch for tab changes - refresh data if switching to a tab with a location
watch(activeTab, async (newTab: string) => {
  if (searchLocation.value.trim() && !loading.value) {
    // When switching tabs, reload data for the active tab
    if (newTab === 'current' && !currentWeather.value) {
      await loadCurrentWeather()
    }
    // For forecast tab, trigger refresh to ensure data loads when tab becomes visible
    else if (newTab === 'forecast') {
      forecastRefreshTrigger.value++
    }
  }
})

// Lifecycle
onMounted(async () => {
  await loadProfile()
  // Load initial data if we have a default location
  if (currentLocation.value && currentLocation.value === profile.value.defaultLocation) {
    searchLocation.value = currentLocation.value // This will trigger data loading
  }
})
</script>

<style scoped>
/* Custom styles if needed */
</style>